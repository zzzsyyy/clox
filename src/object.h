#ifndef clox_object_h
#define clox_object_h

#include "chunk.h"
#include "common.h"
#include "table.h"
#include "value.h"

#define OBJ_TYPE(value)        obj_type(AS_OBJ(value))
#define IS_BOUND_METHOD(value) isObjType(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value)        isObjType(value, OBJ_CLASS)
#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_INSTANCE(value)     isObjType(value, OBJ_INSTANCE)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define AS_BOUND_METHOD(value) ((ObjBoundMethod*)AS_OBJ(value))
#define AS_CLASS(value)        ((ObjClass*)AS_OBJ(value))
#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_INSTANCE(value)     ((ObjInstance*)AS_OBJ(value))
#define AS_NATIVE(value)       (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
	OBJ_BOUND_METHOD,
	OBJ_CLASS,
	OBJ_CLOSURE,
	OBJ_FUNCTION,
	OBJ_INSTANCE,
	OBJ_NATIVE,
	OBJ_STRING,
	OBJ_UPVALUE,
} ObjType;


/*        ┌───┬───┬───┬───┐
Obj       │   │   │   │   │
          ├───┴───┴───┴───┤
          │ ObjType type  │  There exactly line up
          ├───┬───┬───┬───┼───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐
ObjString │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │   │
          ├───┴───┴───┴───┼───┴───┴───┴───┴───┼───┴───┴───┴───┴───┴───┴───┤
          │ Obj obj       │int length         │char* chars                │
          └───────────────┴───────────────────┴───────────────────────────┘
 */

// struct Obj {
// 	ObjType type;
// 	bool is_marked;
// 	struct Obj *next;
// };

/*
.....TTT .......M NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN NNNNNNNN

T = Type enum, M = mark bit, N = next pointer
*/

struct Obj{
	uint64_t header;
};

static inline ObjType obj_type(Obj *object) {
	return (ObjType)((object->header >> 56) & 0xff);
}

static inline bool is_marked(Obj *object) {
	return (bool)((object->header >> 48) & 0x01);
}

static inline Obj* obj_next(Obj *object) {
	return (Obj*)(object->header & 0x0000ffffffffffff);
}

static inline void set_is_marked(Obj* object, bool is_marked) {
	object->header = (object->header & 0xff00ffffffffffff) | ((uint64_t)is_marked << 48);
}

static inline void set_obj_next(Obj *object, Obj *next) {
	object->header = (object->header & 0xffff000000000000) | (uint64_t)next;
}

typedef struct {
	Obj obj;
	int arity;
	int upvalue_count;
	Chunk chunk;
	ObjString *name;
} ObjFunction;

typedef Value (*NativeFn)(int arg_count, Value *args);

typedef struct {
	Obj obj;
	NativeFn function;
} ObjNative;

struct ObjString {
	Obj obj;
	int length;
	char* chars;
	uint32_t hash;
};

typedef struct ObjUpValue {
	Obj obj;
	Value *location;
	Value closed;
	struct ObjUpValue *next;
} ObjUpValue;

typedef struct {
	Obj obj;
	ObjFunction *function;
	ObjUpValue **upvalues;
	int upvalue_count;
} ObjClosure;

typedef struct {
	Obj obj;
	ObjString *name;
	Table methods;
} ObjClass;

typedef struct {
	Obj obj;
	ObjClass *klass;
	Table fields;
} ObjInstance;

typedef struct {
	Obj obj;
	Value receiver;
	ObjClosure *method;
} ObjBoundMethod;

ObjBoundMethod *new_bound_method(Value reveiver, ObjClosure *method);
ObjClass *new_class(ObjString *name);
ObjClosure *new_closure(ObjFunction *function);
ObjFunction *new_function();
ObjInstance *new_instance(ObjClass *klass);
ObjNative *new_native(NativeFn function);
ObjString *takeString(char *chars, int length);
ObjString *copyString(const char *chars, int length);
ObjUpValue *new_upvalue(Value *slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
	return IS_OBJ(value) && obj_type(AS_OBJ(value)) == type;
}

#endif
