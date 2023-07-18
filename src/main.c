#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, char* argv[]){
  Chunk chunk;
  initChunk(&chunk);
  int constant = addConstant(&chunk, 1.2);
  int cs= addConstant(&chunk, -2.2);
  writeChunk(&chunk, OP_CONSTANT, 12);
  writeChunk(&chunk, constant, 12);
  writeChunk(&chunk, OP_CONSTANT, 13);
  writeChunk(&chunk, cs, 13);
  writeChunk(&chunk, OP_RETURN, 13);
  writeChunk(&chunk, OP_RETURN, 13);
  disassembleChunk(&chunk, "test  chunk");
  freeChunk(&chunk);
  return 0;
}
