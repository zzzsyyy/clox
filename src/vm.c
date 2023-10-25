#include "vm.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

VM g_vm;
bool foundConstantLong = false;

static Value clock_native(int arg_count, Value *args) {
	return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void resetStack() {
	g_vm.stackCount  = 0;
	g_vm.frame_count = 0;
}

static void runtimeError(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fputs("\n", stderr);
	for (int i = g_vm.frame_count - 1; i >= 0; i--) {
		CallFrame *frame      = &g_vm.frames[i];
		ObjFunction *function = frame->function;
		size_t instruction    = frame->ip - function->chunk.code - 1;
		fprintf(stderr, "[line %d] in ", getLineByNumber(&function->chunk.lines, instruction));
		if (function->name == NULL) {
			fprintf(stderr, "script\n");
		} else {
			fprintf(stderr, "%s()\n", function->name->chars);
		}
	}
	resetStack();
}

static void define_native(const char *name, NativeFn function) {
	push(OBJ_VAL(copyString(name, (int)strlen(name))));
	push(OBJ_VAL(new_native(function)));
	tableSet(&g_vm.globals, AS_STRING(g_vm.stack[0]), g_vm.stack[1]);
	pop();
	pop();
}

void initVm() {
	g_vm.stack         = NULL;
	g_vm.stackCapacity = 0;
	g_vm.objects       = NULL;
	initTable(&g_vm.globals);
	initTable(&g_vm.strings);
	define_native("clock", clock_native);
	resetStack();
}

void freeVm() {
	FREE_ARRAY(Value, g_vm.stack, g_vm.stackCapacity);
	freeTable(&g_vm.globals);
	freeTable(&g_vm.strings);
	freeObjects();
}

void push(Value value) {
	if (g_vm.stackCount + 1 > g_vm.stackCapacity) {
		int oldCapacity    = g_vm.stackCapacity;
		g_vm.stackCapacity = GROW_CAPACITY(oldCapacity);
		g_vm.stack         = GROW_ARRAY(Value, g_vm.stack, oldCapacity, g_vm.stackCapacity);
	}
	g_vm.stack[g_vm.stackCount] = value;
	g_vm.stackCount++;
}

Value pop() {
	return g_vm.stack[--g_vm.stackCount];
}

static Value peek(int distance) {
	return g_vm.stack[g_vm.stackCount - 1 - distance];
}

static void push_slots(CallFrame *frame, int var_offset) {
	if (frame->slots_count + 1 > frame->slots_capacity) {
		int old_capacity      = frame->slots_capacity;
		frame->slots_capacity = GROW_CAPACITY(old_capacity);
		frame->slots          = GROW_ARRAY(int, frame->slots, old_capacity, frame->slots_capacity);
	}
	frame->slots[frame->slots_count] = var_offset;
	frame->slots_count++;
}

static bool call(ObjFunction *function, int arg_count) {
	if (arg_count != function->arity) {
		runtimeError("Expect %d arguments but got %d.", function->arity, arg_count);
		return false;
	}
	if (g_vm.frame_count == FRAMES_MAX) {
		runtimeError("Stack overflow on call_frames.");
		return false;
	}
	CallFrame *frame      = &g_vm.frames[g_vm.frame_count++];
	frame->function       = function;
	frame->ip             = function->chunk.code;
	frame->slots          = NULL;
	frame->slots_capacity = 0;
	frame->slots_count    = 0;
	push_slots(frame, g_vm.stackCount - arg_count);
	return true;
}

static bool call_value(Value callee, int arg_count) {
	if (IS_OBJ(callee)) {
		switch (OBJ_TYPE(callee)) {
			case OBJ_FUNCTION:
				return call(AS_FUNCTION(callee), arg_count);
			case OBJ_NATIVE: {
				NativeFn native = AS_NATIVE(callee);
				Value result    = native(arg_count, &g_vm.stack[g_vm.stackCount - arg_count - 1]);
				g_vm.stackCount -= arg_count + 1;
				push(result);
				return true;
			}
			default:
				break;
		}
	}
	runtimeError("Can only call functions and classes.");
	return false;
}

static bool isFalsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
	ObjString *b = AS_STRING(pop());
	ObjString *a = AS_STRING(pop());

	int length  = a->length + b->length;
	char *chars = ALLOCATE(char, length + 1);
	memcpy(chars, a->chars, a->length);
	memcpy(chars + a->length, b->chars, b->length);
	chars[length] = '\0';

	ObjString *result = takeString(chars, length);
	push(OBJ_VAL(result));
}

static void testStack(bool boolean) {
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);

	if (boolean) {
		push(NUMBER_VAL(-AS_NUMBER(pop())));
	} else {
		g_vm.stack[g_vm.stackCount - 1] = NUMBER_VAL(-AS_NUMBER(g_vm.stack[g_vm.stackCount - 1]));
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
	double exec_time_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
	// printf("RESULT: %f\n", exec_time_ns);
}

static void print_slots(CallFrame *frame) {
	for (int i = 0; i < frame->slots_count; i++) {
		printf("[%d]", frame->slots[i]);
	}
	printf("\n");
}

static InterpretResult run() {
	CallFrame *frame = &g_vm.frames[g_vm.frame_count - 1];
#define READ_BYTE() (*frame->ip++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())
// Une astuce macro habituelle
#define BINARY_OP(value_type, op)                     \
	do {                                                \
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
			runtimeError("Operands must be numbers.");      \
			return INTERPRET_RUNTIME_ERROR;                 \
		}                                                 \
		double b = AS_NUMBER(pop());                      \
		double a = AS_NUMBER(pop());                      \
		push(value_type(a op b));                         \
	} while (false)

	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("			");
		for (Value *slot = g_vm.stack; slot < g_vm.stack + g_vm.stackCount; slot++) {
			printf("[ ");
			print_value(*slot);
			printf(" ]");
		}
		printf("\n");
		int offset      = (int)(frame->ip - frame->function->chunk.code);
		int prevInstruc = frame->function->chunk.code[offset - 2];
		if (prevInstruc == OP_CONSTANT_LONG && foundConstantLong) {
			// this flag is use to assert prevInstruc is a opCode not a number 7
			// opcode byte byte byte
			frame->ip += 2;
			foundConstantLong = false;
		}
		disassemble_instruction(&frame->function->chunk, (int)(frame->ip - frame->function->chunk.code));
#endif
		uint8_t instruction;
		switch (instruction = READ_BYTE()) {
			case OP_CONSTANT: {
				Value constant = READ_CONSTANT();
				push(constant);
				break;
			}
			case OP_CONSTANT_LONG: {
				foundConstantLong = true;
				Value constant    = READ_CONSTANT();
				push(constant);
				break;
			}
			case OP_NIL:
				push(NIL_VAL);
				break;
			case OP_TRUE:
				push(BOOL_VAL(true));
				break;
			case OP_FALSE:
				push(BOOL_VAL(false));
				break;
			case OP_POP:
				pop();
				break;
			case OP_GET_LOCAL: {
				uint8_t slot = READ_BYTE();
				if (slot > frame->slots_count) {
					push(g_vm.stack[frame->slots[frame->slots_count - 1] + slot - frame->slots_count]);
				} else {
					push(g_vm.stack[frame->slots[slot - 1]]);
				}
				break;
			}
			case OP_SET_LOCAL: {
				uint8_t slot = READ_BYTE();
				// FIXME
				push_slots(frame, g_vm.stackCount - 1);
				break;
			}
			case OP_GET_GLOBAL: {
				ObjString *name = READ_STRING();
				Value value;
				if (!tableGet(&g_vm.globals, name, &value)) {
					runtimeError("Undefined variable '%s'.", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				push(value);
				break;
			}
			case OP_DEFINE_GLOBAL: {
				ObjString *name = READ_STRING();
				tableSet(&g_vm.globals, name, peek(0));
				pop();  // make sure the value be though gc
				break;
			}
			case OP_SET_GLOBAL: {
				ObjString *name = READ_STRING();
				if (tableSet(&g_vm.globals, name, peek(0))) {
					tableDel(&g_vm.globals, name);
					runtimeError("Undefined variable '%s'.", name->chars);
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_EQUAL: {
				Value b = pop();
				Value a = pop();
				push(BOOL_VAL(values_equal(a, b)));
				break;
			}
			case OP_GREATER:
				BINARY_OP(BOOL_VAL, >);
				break;
			case OP_LESS:
				BINARY_OP(BOOL_VAL, <);
				break;
			case OP_ADD: {
				if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
					concatenate();
				} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
					double b = AS_NUMBER(pop());
					double a = AS_NUMBER(pop());
					push(NUMBER_VAL(a + b));
				} else {
					runtimeError("Operands must be two numbers or two strings.");
					return INTERPRET_RUNTIME_ERROR;
				}
				break;
			}
			case OP_SUBTRACT:
				BINARY_OP(NUMBER_VAL, -);
				break;
			case OP_MULTIPLY:
				BINARY_OP(NUMBER_VAL, *);
				break;
			case OP_DIVIDE:
				BINARY_OP(NUMBER_VAL, /);
				break;
			case OP_NOT:
				push(BOOL_VAL(isFalsey(pop())));
				break;
			case OP_NEGATE:
				if (!IS_NUMBER(peek(0))) {
					runtimeError("Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}
				testStack(false);  // no-pop action seems to be faster a little bit
				// testStack(true);
				break;
			case OP_PRINT: {
				print_value(pop());
				printf("\n");
				break;
			}
			case OP_JUMP: {
				uint16_t offset = READ_SHORT();
				frame->ip += offset;
				break;
			}
			case OP_JUMP_IF_FALSE: {
				uint16_t offset = READ_SHORT();
				if (isFalsey(peek(0))) frame->ip += offset;
				break;
			}
			case OP_LOOP: {
				uint16_t offset = READ_SHORT();
				frame->ip -= offset;
				break;
			}
			case OP_CALL: {
				int arg_count = READ_BYTE();
				if (!call_value(peek(arg_count), arg_count)) {
					return INTERPRET_RUNTIME_ERROR;
				}
				frame = &g_vm.frames[g_vm.frame_count - 1];
				break;
			}
			case OP_RETURN: {
				Value result = pop();
				g_vm.frame_count--;
				if (g_vm.frame_count == 0) {
					pop();
					return INTERPRET_OK;
				}

				g_vm.stackCount = frame->slots[0] - 1;

				push(result);
				frame = &g_vm.frames[g_vm.frame_count - 1];
				break;
			}
		}
	}
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *src) {
	ObjFunction *function = compile(src);
	if (function == NULL) return INTERPRET_COMPILE_ERROR;

	push(OBJ_VAL(function));
	call(function, 0);
	return run();
}
