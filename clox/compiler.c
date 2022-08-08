#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

bool compile(const char* source, Chunk* chunk) {
    Scanner scanner;
    Parser parser;
    initScanner(&scanner, source);
    advance(&scanner, &parser);
    expression();
    consume(TOKEN_EOF, "Expect end of expression.");
}
