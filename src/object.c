#include "object.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "chunk.h"
#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) (type *)allocateObject(sizeof(type), objectType)

static Obj *allocateObject(size_t size, ObjType type) {
	Obj *object       = (Obj *)reallocate(NULL, 0, size);
	object->type      = type;
	object->is_marked = false;
	object->next      = g_vm.objects;
	g_vm.objects      = object;
#ifdef DEBUG_LOG_GC
	printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif
	return object;
}

ObjClosure *new_closure(ObjFunction *function) {
	ObjUpValue **upvalues = ALLOCATE(ObjUpValue *, function->upvalue_count);
	for (int i = 0; i < function->upvalue_count; i++) {
		upvalues[i] = NULL;
	}
	ObjClosure *closure    = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
	closure->function      = function;
	closure->upvalues      = upvalues;
	closure->upvalue_count = function->upvalue_count;
	return closure;
}

ObjFunction *new_function() {
	ObjFunction *function   = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
	function->arity         = 0;
	function->upvalue_count = 0;
	function->name          = NULL;
	init_chunk(&function->chunk);
	return function;
}

ObjNative *new_native(NativeFn function) {
	ObjNative *native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
	native->function  = function;
	return native;
}

static uint32_t hashString(const char *key, int length) {
	uint32_t hash = 0x811c9dc5u;
	for (int i = 0; i < length; i++) {
		hash ^= (uint8_t)key[i];
		hash *= 0x01000193;
	}
	return hash;
}

// Something like `new` func in OOP
static ObjString *allocateString(char *chars, int length, uint32_t hash) {
	ObjString *string = ALLOCATE_OBJ(ObjString, OBJ_STRING);
	string->length    = length;
	string->chars     = chars;
	string->hash      = hash;
	push(OBJ_VAL(string));
	tableSet(&g_vm.strings, string, NIL_VAL);
	pop();
	return string;
}

ObjString *takeString(char *chars, int length) {
	uint32_t hash       = hashString(chars, length);
	ObjString *interned = tableFindString(&g_vm.strings, chars, length, hash);
	if (interned != NULL) {
		FREE_ARRAY(char, chars, length + 1);
		return interned;
	}
	return allocateString(chars, length, hash);
}

ObjString *copyString(const char *chars, int length) {
	uint32_t hash       = hashString(chars, length);
	ObjString *interned = tableFindString(&g_vm.strings, chars, length, hash);
	if (interned != NULL) return interned;
	char *heapChars = ALLOCATE(char, length + 1);
	memcpy(heapChars, chars, length);
	heapChars[length] = '\0';
	return allocateString(heapChars, length, hash);
}

ObjUpValue *new_upvalue(Value *slot) {
	ObjUpValue *upvalue = ALLOCATE_OBJ(ObjUpValue, OBJ_UPVALUE);
	upvalue->closed     = NIL_VAL;
	upvalue->location   = slot;
	upvalue->next       = NULL;
	return upvalue;
}

static void print_function(ObjFunction *function) {
	if (function->name == NULL) {
		printf("<script>");
		return;
	}
	printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
	switch (OBJ_TYPE(value)) {
		case OBJ_CLOSURE:
			print_function(AS_CLOSURE(value)->function);
			break;
		case OBJ_FUNCTION:
			print_function(AS_FUNCTION(value));
			break;
		case OBJ_NATIVE:
			printf("<native fn>");
			break;
		case OBJ_STRING:
			printf("%s", AS_CSTRING(value));
			break;
		case OBJ_UPVALUE:
			printf("upvalue");
			break;
	}
}
