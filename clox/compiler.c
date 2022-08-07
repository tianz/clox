#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

void compile(const char* source) {
    Scanner scanner;
    initScanner(&scanner, source);
}