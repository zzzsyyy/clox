#include "chunk.h"

#include <stdint.h>
#include <stdlib.h>

#include "memory.h"
#include "vm.h"

void init_chunk(Chunk *chunk) {
	chunk->count    = 0;
	chunk->capacity = 0;
	chunk->code     = NULL;
	initLines(&chunk->lines);
	init_value_array(&chunk->constants);
}

void write_chunk(Chunk *chunk, uint8_t byte, int line) {
	if (chunk->capacity < chunk->count + 1) {
		int oldCapacity = chunk->capacity;
		chunk->capacity = GROW_CAPACITY(oldCapacity);
		chunk->code     = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
	}
	chunk->code[chunk->count] = byte;
	writeLines(&chunk->lines, line);
	chunk->count++;
}

int add_constant(Chunk *chunk, Value value) {
	push(value);
	write_value_array(&chunk->constants, value);
	pop();
	return chunk->constants.count - 1;
}

void free_chunk(Chunk *chunk) {
	FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
	freeLines(&chunk->lines);
	free_value_array(&chunk->constants);
	init_chunk(chunk);
}
