#include "chunk.h"
#include "common.h"
#include "debug.h"
#include "vm.h"
// #include <stdio.h>

int main(int argc, char *argv[]) {
	init_vm();
	Chunk chunk;
	init_chunk(&chunk);
	int constant = add_constant(&chunk, 1.2);
  write_chunk(&chunk, OP_CONSTANT, 123);
  write_chunk(&chunk, constant, 123);
  constant = add_constant(&chunk, 3.4);
  write_chunk(&chunk, OP_CONSTANT, 123);
  write_chunk(&chunk, constant, 123);
  write_chunk(&chunk, OP_ADD, 123);
  constant = add_constant(&chunk, 5.6);
  write_chunk(&chunk, OP_CONSTANT, 123);
  write_chunk(&chunk, constant, 123);
  write_chunk(&chunk, OP_DIVIDE, 123);
  write_chunk(&chunk, OP_NEGATE, 123);
  write_chunk(&chunk, OP_RETURN, 123);
	// printf("%d\n", chunk.capacity);
	// printf("%d\n", chunk.count);
	// for (int i=0; i < chunk.lines.capacity; i++){
	// 	printf("offset %d -> %d\t%d\n", i, chunk.lines.lines[i], chunk.lines.times[i]);
	// }
	// for (int i=0;i<sizeof(chunk.constants.capacity);i++){
	// 	printf("%f\n", chunk.constants.values[i]);
	// }
	// printf("%d", *chunk.lines);
	// disassemble_chunk(&chunk, "test chunk");
	interpret(&chunk);
	free_vm();
	free_chunk(&chunk);
	return 0;
}
