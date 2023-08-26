#include "vm.h"
#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "value.h"
#include <stdarg.h>
#include <stdio.h>
#include "compiler.h"

VM vm;
bool foundConstantLong = false;

static void reset_stack() { vm.stack_top = vm.stack; }

static void runtime_err(const char* format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);
	size_t instruction = vm.ip - vm.chunk->code - 1;
  int line = getLineByNumber(&vm.chunk->lines, instruction);
  fprintf(stderr, "[line %d] in script\n", line);
	reset_stack();
}

void init_vm() { reset_stack(); }

void free_vm() {}

void push(Value value) {
	*vm.stack_top = value;
	vm.stack_top++;
}

Value pop() {
	vm.stack_top--;
	return *vm.stack_top;
}

static Value peek(int distance) {
	return vm.stack_top[-1-distance];
}

static bool is_falsey(Value value) {
	return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
//Une astuce macro habituelle
#define BINARY_OP(value_type, op) \
	do { \
		if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
			runtime_err("Operands must be numbers."); \
			return INTERPRET_RUNTIME_ERROR; \
		} \
		double b = AS_NUMBER(pop()); \
		double a = AS_NUMBER(pop()); \
		push(value_type(a op b)); \
	} while(false)

	for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
		printf("			");
		for (Value *slot = vm.stack; slot < vm.stack_top; slot++) {
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
		case OP_ADD:      BINARY_OP(NUMBER_VAL, +); break;
		case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
		case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
		case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
		case OP_NOT: push(BOOL_VAL(is_falsey(pop()))); break;
		case OP_NEGATE:
			if (!IS_NUMBER(peek(0))) {
					runtime_err("Operand must be a number.");
					return INTERPRET_RUNTIME_ERROR;
				}
			push(NUMBER_VAL(-AS_NUMBER(pop())));
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
