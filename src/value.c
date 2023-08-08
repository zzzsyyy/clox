#include <stdio.h>

#include "memory.h"
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
	}
}

bool values_equal(Value a, Value b) {
	if (a.type != b.type) return false;
	switch (a.type) {
		case VAL_BOOL:   return AS_BOOL(a) == AS_BOOL(b);
		case VAL_NIL:    return true;
		case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
		default:         return false;
	}
}

void init_lines(Lignes *lines){
	lines->capacity = 0;
	lines->count = -1;
	lines->lines = NULL;
	lines->times = NULL;
}

void write_lines(Lignes *lines, int line){
	if (lines->capacity	<= lines->count + 1){
		int old_capacity = lines->capacity;
		lines->capacity = GROW_CAPACITY(old_capacity);
		lines->times = GROW_ARRAY(int, lines->times, old_capacity, lines->capacity);
		lines->lines = GROW_ARRAY(int, lines->lines, old_capacity, lines->capacity);
	}
	if (lines->count == -1 || lines->lines[lines->count] != line){
		lines->count++;
		lines->lines[lines->count] = line;
		lines->times[lines->count] = 1;
	}else{
		lines->times[lines->count]++;
	}
}

void free_lines(Lignes *lines){
	FREE_ARRAY(int, lines->lines, lines->capacity);
	FREE_ARRAY(int, lines->times, lines->capacity);
	init_lines(lines);
}

int get_line_by_num(Lignes *lines, int num) {
	if (num > lines->count){
		return -1;
	}
	int idx = 0;
	int cur = 0;
	if (idx < num){
		idx+=lines->times[cur++];
	}
	return lines->lines[idx-1];
}
