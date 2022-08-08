#ifndef clox_compiler_h
#define clox_compiler_h

#include "scanner.h"
#include "vm.h"

typedef struct {
    Token current;
    Token previous;
} Parser;

bool compile(const char* source, Chunk* chunk);

#endif
