#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

typedef struct {
	Chunk *chunk;
	uint8_t *ip; //Program Counter (PC)
	Value *stack;
	int stackCapacity;
	int stackCount;
	Table strings;
	Obj *objects;
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVm();
void freeVm();
InterpretResult interpret(const char *src);
void push(Value value);
Value pop();

#endif
