#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

static void advance(Scanner* scanner, Parser* parser);
static void consume(Scanner* scanner, Parser* parser, TokenType type, const char* message);
static void expression();
static void grouping(Scanner* scanner, Parser* parser);
static void number(Parser* parser);
static void unary(Parser* parser);
static void endCompiler(Parser* parser);
static void emitReturn(Parser* parser);
static void emitConstant(Parser* parser, Value value);
static void emitByte(Parser* parser, uint8_t byte);
static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2);
static Chunk* currentChunk();
static void error(Parser* parser, const char* message);
static void errorAtCurrent(Parser* parser, const char* message);
static void errorAt(Parser* parser, Token* token, const char* message);

Chunk* compilingChunk;

bool compile(const char* source, Chunk* chunk) {
    Scanner scanner;
    Parser parser;

    // initialize scanner, chunk and parser
    initScanner(&scanner, source);
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;

    advance(&scanner, &parser);
    expression();
    consume(&scanner, &parser, TOKEN_EOF, "Expect end of expression.");
    endCompiler(&parser);

    return !parser.hadError;
}

static void advance(Scanner* scanner, Parser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = scanToken(scanner);

        // iterate over the error tokens and report the errors
        if (parser->current.type != TOKEN_ERROR) {
            break;
        }

        errorAtCurrent(parser, parser->current.start);
    }
}

static void consume(Scanner* scanner, Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(scanner, parser);
        return;
    }

    errorAtCurrent(parser, message);
}

static void expression() {
    // parse the lowest precedence level, which subsumes all of the higher-precedence expressions too
    parsePrecedence(PREC_ASSIGNMENT);
}

static void grouping(Scanner* scanner, Parser* parser) {
    expression();
    consume(scanner, parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(Parser* parser) {
    double value = strtod(parser->previous.start, NULL);
    emitConstant(parser, value);
}

static void unary(Parser* parser) {
    TokenType operatorType = parser->previous.type;

    // compile the operand
    parsePrecedence(PREC_UNARY);

    // emit the operator instruction
    switch (operatorType) {
        case TOKEN_MINUS:
            emitByte(parser, OP_NEGATE);
            break;
        default:
            // unreachable
            return;
    }
}

static void parsePrecedence(Precedence precedence) {

}

static void endCompiler(Parser* parser) {
    emitReturn(parser);
}

static void emitReturn(Parser* parser) {
    emitByte(parser, OP_RETURN);
}

static void emitConstant(Parser* parser, Value value) {
    emitBytes(parser, OP_CONSTANT, makeConstant(parser, value));
}

static uint8_t makeConstant(Parser* parser, Value value) {
    int constant = addConstant(currentChunk(), value);

    if (constant > UINT8_MAX) {
        error(parser, "Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitByte(Parser* parser, uint8_t byte) {
    writeChunk(currentChunk(), byte, parser->previous.line);
}

static void emitBytes(Parser* parser, uint8_t byte1, uint8_t byte2) {
    emitByte(parser, byte1);
    emitByte(parser, byte2);
}

static Chunk* currentChunk() {
    return compilingChunk;
}

static void error(Parser* parser, const char* message) {
    errorAt(parser, &parser->previous, message);
}

static void errorAtCurrent(Parser* parser, const char* message) {
    errorAt(parser, &parser->current, message);
}

static void errorAt(Parser* parser, Token* token, const char* message) {
    if (parser->panicMode) {
        return;
    }
    parser->panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser->hadError = true;
}
