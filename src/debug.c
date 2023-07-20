#include <stdint.h>
#include <stdio.h>

#include "chunk.h"
#include "debug.h"
#include "value.h"

void disassemble_chunk(Chunk *chunk, const char *name) {
	printf("==%s==\n", name);

	for (int offset = 0; offset < chunk->count;) {
		offset = disassemble_instruction(chunk, offset);
	}
}

static int constant_instruction(const char *name, Chunk *chunk, int offset) {
	uint8_t const_idx = chunk->code[offset + 1];
	printf("%s\t%4d '", name, const_idx);
	print_value(chunk->constants.values[const_idx]);
	printf("'\n");
	return offset + 2;
}

static int constant_long_instruction(const char *name,Chunk *chunk, int offset){
	// @TODO
	uint8_t const_idx = chunk->code[offset + 1];
	printf("%s\t%4d '", name, const_idx);
	print_value(chunk->constants.values[const_idx]);
	printf("'\n");
	return offset + 2;
}

static int simple_instruction(const char *name, int offset) {
	printf("%s\n", name);
	return offset + 1;
}

int disassemble_instruction(Chunk *chunk, int offset) {
	printf("%04d ", offset);

	if (offset > 0 && get_line(chunk, offset) == get_line(chunk, offset - 1)) {
		printf("\t| ");
	} else {
		printf("%4d ", get_line(chunk, offset));
	}
	uint8_t instruction = chunk->code[offset];
	switch (instruction) {
	case OP_CONSTANT:
		return constant_instruction("OP_CONSTANT", chunk, offset);
	case OP_CONSTANT_LONG:
		return constant_long_instruction("OP_CONSTANT_LONG", chunk, offset);
	case OP_NEGATE:
		return simple_instruction("OP_NEGATE", offset);
	case OP_ADD:
		return simple_instruction("OP_ADD", offset);
	case OP_SUBTRACT:
		return simple_instruction("OP_SUBTRACT", offset);
	case OP_MULTIPLY:
		return simple_instruction("OP_MULTIPLY", offset);
	case OP_DIVIDE:
		return simple_instruction("OP_DIVIDE", offset);
	case OP_RETURN:
		return simple_instruction("OP_RETURN", offset);
	default:
		printf("Unknow opcode %d\n", instruction);
		return offset + 1;
	}
}
