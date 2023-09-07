#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "compiler.h"

VM vm;
bool foundConstantLong = false;

static void resetStack() { 
	vm.stackCount = 0;
}

static void runtimeError(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
	size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = getLineByNumber(&vm.chunk->lines, instruction);
  fprintf(stderr, "[line %d] in script\n", line);
	resetStack();
}

void initVm() {
	vm.stack = NULL;
	vm.stackCapacity = 0;
	vm.objects = NULL;
	resetStack();
}

void freeVm() {
	FREE_ARRAY(Value, vm.stack, vm.stackCapacity);
	freeObjects();
}

void push(Value value) {
	if (vm.stackCount + 1 > vm.stackCapacity) {
		int oldCapacity = vm.stackCapacity;
		vm.stackCapacity = GROW_CAPACITY(oldCapacity);
		vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackCapacity);
	}
	vm.stack[vm.stackCount] = value;
	vm.stackCount++;
}

Value pop() {
	return vm.stack[--vm.stackCount];
}

static Value peek(int distance) {
	return vm.stack[vm.stackCount - 1 -distance];
}

static bool isFalsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
	ObjString *b = AS_STRING(pop());
	ObjString *a = AS_STRING(pop());

	int length = a->length + b->length;
	char *chars = ALLOCATE(char, length+1);
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
		vm.stack[vm.stackCount - 1] = NUMBER_VAL(- AS_NUMBER(vm.stack[vm.stackCount - 1]));
	}

	clock_gettime(CLOCK_MONOTONIC, &end);
	double exec_time_ns = (end.tv_sec - start.tv_sec) * 1e9 + (end.tv_nsec - start.tv_nsec);
	printf("RESULT: %f\n", exec_time_ns);
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
//Une astuce macro habituelle
#define BINARY_OP(value_type, op) \
	do { \
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
			runtimeError("Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		} \
		double b = AS_NUMBER(pop()); \
		double a = AS_NUMBER(pop()); \
		push(value_type(a op b)); \
	} while(false)

	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("			");
		for (Value *slot = vm.stack; slot < vm.stack + vm.stackCount; slot++) {
			printf("[ ");
			print_value(*slot);
			printf(" ]");
		}
		printf("\n");
		int offset = (int)(vm.ip - vm.chunk->code);
		int prevInstruc = vm.chunk->code[offset - 2];
		if (prevInstruc == OP_CONSTANT_LONG && foundConstantLong) {
			// this flag is use to assert prevInstruc is a opCode not a number 7
			// opcode byte byte byte
			vm.ip += 2; 
			foundConstantLong = false;
		}
		disassemble_instruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
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
			Value constant = READ_CONSTANT();
			push(constant);
			break;
		}
		case OP_NIL: push(NIL_VAL); break;
		case OP_TRUE: push(BOOL_VAL(true)); break;
		case OP_FALSE: push(BOOL_VAL(false)); break;
		case OP_EQUAL: {
			Value b = pop();
			Value a = pop();
			push (BOOL_VAL(values_equal(a, b)));
			break;
		}
		case OP_GREATER:  BINARY_OP(BOOL_VAL, >);   break;
		case OP_LESS:     BINARY_OP(BOOL_VAL, <);   break;
		case OP_ADD: {
			if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
				concatenate();
			} else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
				double b = AS_NUMBER(pop());
				double a = AS_NUMBER(pop());
				push(NUMBER_VAL(a +  b));
			} else {
				runtimeError("Operands must be two numbers or two strings.");
				return INTERPRET_RUNTIME_ERROR;
			}
			break;
		}
		case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
		case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
		case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
		case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;
		case OP_NEGATE:
			if (!IS_NUMBER(peek(0))) {
					runtimeError("Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}
			testStack(false); // no-pop action seems to be faster a little bit
			// testStack(true);
			break;
		case OP_RETURN: {
			print_value(pop());
			printf("\n");
			return INTERPRET_OK;
		}
		}
	}
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char *src) {
	Chunk chunk;
	init_chunk(&chunk);
	if (!compile(src, &chunk)) {
		free_chunk(&chunk);
		return INTERPRET_COMPILE_ERROR;
	}
	vm.chunk = &chunk;
	vm.ip = vm.chunk->code;
	InterpretResult result = run();
	free_chunk(&chunk);
	return result;
}
