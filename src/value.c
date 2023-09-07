#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "value.h"
#include "chunk.h"

void init_value_array(ValueArray *array) {
	array->values = NULL;
	array->capacity = 0;
	array->count = 0;
}

void write_value_array(ValueArray *array, Value value) {
	if (array->capacity <= array->count + 1) {
		int old_capacity = array->capacity;
		array->capacity = GROW_CAPACITY(old_capacity);
		array->values =
				GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
	}

	array->values[array->count] = value;
	array->count++;
}

void free_value_array(ValueArray *array) {
	FREE_ARRAY(Value, array->values, array->capacity);
	init_value_array(array);
}

void print_value(Value value) {
	switch (value.type){
		case VAL_BOOL:
			printf(AS_BOOL(value) ? "true" : "false");
			break;
		case VAL_NIL: printf("nil"); break;
		case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
		case VAL_OBJ: printObject(value); break;
	}
}

bool values_equal(Value a, Value b) {
	if (a.type != b.type) return false;
	switch (a.type) {
		case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
		case VAL_NIL:    return true;
		case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
		case VAL_OBJ: {
			ObjString *aString = AS_STRING(a);
			ObjString *bString = AS_STRING(b);
			return aString->length == bString->length &&
				memcmp(aString->chars, bString->chars, aString->length)==0;
		}
		default:         return false;
	}
}

void initLines(Lignes *lines){
	lines->capacity = 0;
	lines->count = -1;
	lines->runs = NULL;
}

void writeLines(Lignes *lines, int line){
	if (lines->capacity	<= lines->count + 1){
		int old_capacity = lines->capacity;
		lines->capacity = GROW_CAPACITY(old_capacity);
		lines->runs = GROW_ARRAY(Run, lines->runs, old_capacity, lines->capacity);
	}

	if (lines->count == -1) {
		lines->count = 0;
		lines->runs[0].lineNumber = line;
		lines->runs[0].runLength = 1;
	} else if (line == lines->runs[lines->count].lineNumber){
		lines->runs[lines->count].runLength+=1;
	} else {
		lines->count++;
		lines->runs[lines->count].lineNumber = line;
		lines->runs[lines->count].runLength = 1;
	}
}

void freeLines(Lignes *lines){
	FREE_ARRAY(Run, lines->runs, lines->capacity);
	initLines(lines);
}

int getLineByNumber(Lignes *lines, int num) {
	int idx = 0;
	while (num-lines->runs[idx].runLength>=0 && idx<=lines->count){
		num-=lines->runs[idx].runLength;
		idx++;
	}
	return lines->count - idx >= 0 ? lines->runs[idx].lineNumber : -1;
}
