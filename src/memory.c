#include "memory.h"

#include <stddef.h>
#include <stdlib.h>

#include "chunk.h"
#include "object.h"
#include "value.h"
#include "vm.h"

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
	if (newSize == 0) {
		free(pointer);
		return NULL;
	}

	void *result = realloc(pointer, newSize);
	if (result == NULL) exit(1);
	return result;
}

static void freeObject(Obj *object) {
	switch (object->type) {
		case OBJ_CLOSURE: {
			ObjClosure *closure = (ObjClosure *)object;
			FREE_ARRAY(ObjUpValue *, closure->upvalues, closure->upvalue_count);
			FREE(ObjClosure, object);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction *function = (ObjFunction *)object;
			free_chunk(&function->chunk);
			FREE(ObjFunction, object);
			break;
		}
		case OBJ_NATIVE: {
			FREE(ObjNative, object);
			break;
		}
		case OBJ_STRING: {
			ObjString *string = (ObjString *)object;
			FREE_ARRAY(char, string->chars, string->length + 1);
			FREE(ObjString, object);
			break;
		}
		case OBJ_UPVALUE:
			FREE(ObjUpValue, object);
			break;
	}
}

void freeObjects() {
	Obj *object = g_vm.objects;
	while (object != NULL) {
		Obj *next = object->next;
		freeObject(object);
		object = next;
	}
}
