#include "memory.h"

#include <stddef.h>
#include <stdlib.h>

#include "chunk.h"
#include "compiler.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>

#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, size_t oldSize, size_t newSize) {
	g_vm.bytes_allocated += newSize - oldSize;
	if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
		collect_garbage();
#endif
		if (g_vm.bytes_allocated > g_vm.next_gc) {
			collect_garbage();
		}
	}
	if (newSize == 0) {
		free(pointer);
		return NULL;
	}

	void *result = realloc(pointer, newSize);
	if (result == NULL) exit(1);
	return result;
}

void mark_object(Obj *object) {
	if (object == NULL) return;
	if (is_marked(object)) return;
#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void *)object);
	print_value(OBJ_VAL(object));
	printf("\n");
#endif
	set_is_marked(object, true);
	if (g_vm.gray_capacity < g_vm.gray_count + 1) {
		g_vm.gray_capacity = GROW_CAPACITY(g_vm.gray_capacity);
		g_vm.gray_stack    = (Obj **)realloc(g_vm.gray_stack, sizeof(Obj *) * g_vm.gray_capacity);
		if (g_vm.gray_stack == NULL) exit(1);
	}
	g_vm.gray_stack[g_vm.gray_count++] = object;
}

void mark_value(Value value) {
	if (IS_OBJ(value)) mark_object(AS_OBJ(value));
}

static void mark_array(ValueArray *array) {
	for (int i = 0; i < array->count; i++) {
		mark_value(array->values[i]);
	}
}

static void blacken_object(Obj *object) {
#ifdef DEBUG_LOG_GC
	printf("%p blacken ", (void *)object);
	print_value((OBJ_VAL(object)));
	printf("\n");
#endif
	switch (obj_type(object)) {
		case OBJ_BOUND_METHOD: {
			ObjBoundMethod *bound = (ObjBoundMethod *)object;
			mark_value(bound->receiver);
			mark_object((Obj *)bound->method);
			break;
		}
		case OBJ_CLASS: {
			ObjClass *klass = (ObjClass *)object;
			mark_object((Obj *)klass->name);
			mark_table(&klass->methods);
			break;
		}
		case OBJ_CLOSURE: {
			ObjClosure *closure = (ObjClosure *)object;
			mark_object((Obj *)closure->function);
			for (int i = 0; i < closure->upvalue_count; i++) {
				mark_object((Obj *)closure->upvalues[i]);
			}
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction *function = (ObjFunction *)object;
			mark_object((Obj *)function->name);
			mark_array(&function->chunk.constants);
			break;
		}
		case OBJ_INSTANCE: {
			ObjInstance *instance = (ObjInstance *)object;
			mark_object((Obj *)instance->klass);
			mark_table(&instance->fields);
			break;
		}
		case OBJ_UPVALUE:
			mark_value(((ObjUpValue *)object)->closed);
			break;
		case OBJ_NATIVE:
		case OBJ_STRING:
			break;
	}
}

static void freeObject(Obj *object) {
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void *)object, obj_type(object));
#endif
	switch (obj_type(object)) {
		case OBJ_BOUND_METHOD:
			FREE(ObjBoundMethod, object);
			break;
		case OBJ_CLASS: {
			ObjClass *klass = (ObjClass *)object;
			freeTable(&klass->methods);
			FREE(ObjClass, object);
			break;
		}
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
		case OBJ_INSTANCE: {
			ObjInstance *instance = (ObjInstance *)object;
			freeTable(&instance->fields);
			FREE(ObjInstance, object);
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

static void mark_roots() {
	for (uint8_t i = 0; i < g_vm.stackCount; i++) {
		mark_value(g_vm.stack[i]);
	}
	for (int i = 0; i < g_vm.frame_count; i++) {
		mark_object((Obj *)g_vm.frames[i].closure);
	}
	for (ObjUpValue *upvalue = g_vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next) {
		mark_object((Obj *)upvalue);
	}
	mark_table(&g_vm.globals);
	mark_compiler_roots();
	mark_object((Obj *)g_vm.init_string);
}

static void trace_refs() {
	while (g_vm.gray_count > 0) {
		Obj *object = g_vm.gray_stack[--g_vm.gray_count];
		blacken_object(object);
	}
}

static void sweep() {
	Obj *previous = NULL;
	Obj *object   = g_vm.objects;
	while (object != NULL) {
		if (is_marked(object)) {
			set_is_marked(object, false);
			previous = object;
			object   = obj_next(object);
		} else {
			Obj *unreached = object;
			object         = obj_next(object);
			if (previous != NULL) {
				set_obj_next(previous, object);
			} else {
				g_vm.objects = object;
			}
			freeObject(unreached);
		}
	}
}

void collect_garbage() {
#ifdef DEBUG_LOG_GC
	printf("-- gc begin\n");
	size_t before = g_vm.bytes_allocated;
#endif
	mark_roots();
	trace_refs();
	table_rm_white(&g_vm.strings);
	sweep();
	g_vm.next_gc = g_vm.bytes_allocated * GC_HEAP_GROW_FACTOR;
#ifdef DEBUG_LOG_GC
	printf("-- gc end\n");
	printf("   collected %zu bytes (de %zu à %zu) next at %zu\n", before - g_vm.bytes_allocated, before,
	       g_vm.bytes_allocated, g_vm.next_gc);
#endif
}

void freeObjects() {
	Obj *object = g_vm.objects;
	while (object != NULL) {
		Obj *next = obj_next(object);
		freeObject(object);
		object = next;
	}
	free(g_vm.gray_stack);
}
