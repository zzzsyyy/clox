#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;

typedef struct {
	int capacity;
	int count;
	int *lines;
	int *times;
} Lignes;

typedef struct {
	int capacity;
	int count;
	Value *values;
} ValueArray;

void init_value_array(ValueArray *array);
void write_value_array(ValueArray *array, Value value);
void free_value_array(ValueArray *array);
void print_value(Value value);

void init_lines(Lignes *lines);
void write_lines(Lignes *lines, int line);
void free_lines(Lignes *lines);

#endif
