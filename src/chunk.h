#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "memory.h"
#include "value.h"

typedef enum {
	OP_CONSTANT,
	OP_CONSTANT_LONG,
	OP_RETURN,
} OpCode;

typedef struct {
	int count;
	int capacity;
	uint8_t *code;
	ValueArray constants;
	Lignes lines;
} Chunk;

void init_chunk(Chunk *chunk);
void write_chunk(Chunk *chunk, uint8_t byte, int line);
int add_constant(Chunk *chunk, Value value);
void free_chunk(Chunk *chunk);
int get_line(Chunk *chunk, int idx);

#endif
