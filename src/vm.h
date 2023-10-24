#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
//#define STACK_MAX (FRAMES_MAX * UINT8_*_COUNT)

typedef struct {
	ObjFunction *function;
	uint8_t *ip;
	Value *slots;
} CallFrame;

typedef struct {
	CallFrame frames[FRAMES_MAX];
	int frame_count;
	Value *stack;
	int stackCapacity;
	int stackCount;
	Table globals;
	Table strings;
	Obj *objects;
} VM;

typedef enum {
	INTERPRET_OK,
	INTERPRET_COMPILE_ERROR,
	INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM g_vm;

void initVm();
void freeVm();
InterpretResult interpret(const char *src);
void push(Value value);
Value pop();

#endif
