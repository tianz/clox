#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

static void initCompiler(Compiler* compiler);
static void advance();
static void consume(TokenType type, const char* message);
static bool match(TokenType type);
static bool check(TokenType type);
static void declaration();
static void varDeclaration();
static uint8_t parseVariable(const char* errorMessage);
static uint8_t identifierConstant(Token* name);
static void defineVariable(uint8_t global);
static void statement();
static void beginScope();
static void endScope();
static void printStatement();
static void expressionStatement();
static void block();
static void expression();
static void grouping(bool canAssign);
static void binary(bool canAssign);
static void unary(bool canAssign);
static void variable(bool canAssign);
static void namedVariable(Token name, bool canAssign);
static void number(bool canAssign);
static void literal(bool canAssign);
static void string(bool canAssign);
static void parsePrecedence(Precedence precedence);
static ParseRule* getRule(TokenType type);
static void endCompiler();
static void emitReturn();
static void emitConstant(Value value);
static uint8_t makeConstant(Value value);
static void emitByte(uint8_t byte);
static void emitBytes(uint8_t byte1, uint8_t byte2);
static Chunk* currentChunk();
static void error(const char* message);
static void errorAtCurrent(const char* message);
static void errorAt(Token* token, const char* message);
static void synchronize();

Scanner scanner;
Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN] = { grouping, NULL, PREC_NONE},
    [TOKEN_RIGHT_PAREN] = { NULL, NULL, PREC_NONE},
    [TOKEN_LEFT_BRACE] = { NULL, NULL, PREC_NONE},
    [TOKEN_RIGHT_BRACE] = { NULL, NULL, PREC_NONE},
    [TOKEN_COMMA] = { NULL, NULL, PREC_NONE},
    [TOKEN_DOT] = { NULL, NULL, PREC_NONE},
    [TOKEN_MINUS] = { unary, binary, PREC_TERM},
    [TOKEN_PLUS] = { NULL, binary, PREC_TERM},
    [TOKEN_SEMICOLON] = { NULL, NULL, PREC_NONE},
    [TOKEN_SLASH] = { NULL, binary, PREC_FACTOR},
    [TOKEN_STAR] = { NULL, binary, PREC_FACTOR},
    [TOKEN_BANG] = { unary, NULL, PREC_NONE},
    [TOKEN_BANG_EQUAL] = { NULL, binary, PREC_EQUALITY},
    [TOKEN_EQUAL] = { NULL, NULL, PREC_NONE},
    [TOKEN_EQUAL_EQUAL] = { NULL, binary, PREC_EQUALITY},
    [TOKEN_GREATER] = { NULL, binary, PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL] = { NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS] = { NULL, binary, PREC_COMPARISON},
    [TOKEN_LESS_EQUAL] = { NULL, binary, PREC_COMPARISON},
    [TOKEN_IDENTIFIER] = { variable, NULL, PREC_NONE},
    [TOKEN_STRING] = { string, NULL, PREC_NONE},
    [TOKEN_NUMBER] = { number, NULL, PREC_NONE},
    [TOKEN_AND] = { NULL, NULL, PREC_NONE},
    [TOKEN_CLASS] = { NULL, NULL, PREC_NONE},
    [TOKEN_ELSE] = { NULL, NULL, PREC_NONE},
    [TOKEN_FALSE] = { literal, NULL, PREC_NONE},
    [TOKEN_FOR] = { NULL, NULL, PREC_NONE},
    [TOKEN_FUN] = { NULL, NULL, PREC_NONE},
    [TOKEN_IF] = { NULL, NULL, PREC_NONE},
    [TOKEN_NIL] = { literal, NULL, PREC_NONE},
    [TOKEN_OR] = { NULL, NULL, PREC_NONE},
    [TOKEN_PRINT] = { NULL, NULL, PREC_NONE},
    [TOKEN_RETURN] = { NULL, NULL, PREC_NONE},
    [TOKEN_SUPER] = { NULL, NULL, PREC_NONE},
    [TOKEN_THIS] = { NULL, NULL, PREC_NONE},
    [TOKEN_TRUE] = { literal, NULL, PREC_NONE},
    [TOKEN_VAR] = { NULL, NULL, PREC_NONE},
    [TOKEN_WHILE] = { NULL, NULL, PREC_NONE},
    [TOKEN_ERROR] = { NULL, NULL, PREC_NONE},
    [TOKEN_EOF] = { NULL, NULL, PREC_NONE},
};

bool compile(const char* source, Chunk* chunk) {
    // initialize compiler, scanner, chunk and parser
    Compiler compiler;
    initCompiler(&compiler);
    initScanner(&scanner, source);
    compilingChunk = chunk;
    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    endCompiler();

    return !parser.hadError;
}

static void initCompiler(Compiler* compiler) {
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    current = compiler;
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken(&scanner);

        // iterate over the error tokens and report the errors
        if (parser.current.type != TOKEN_ERROR) {
            break;
        }

        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool match(TokenType type) {
    if (!check(type)) {
        return false;
    }
    advance();
    return true;
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static void declaration() {
    if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        statement();
    }
}

static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name."); // the index of the variable name in the constants array

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

static uint8_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);
    return identifierConstant(&parser.previous);
}

// Returns the index where the identifier constant is added.
static uint8_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static void defineVariable(uint8_t global) {
    emitBytes(OP_DEFINE_GLOBAL, global);
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void expression() {
    // parse the lowest precedence level, which subsumes all of the higher-precedence expressions too
    parsePrecedence(PREC_ASSIGNMENT);
}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule* rule = getRule(operatorType);
    // compile the right operand
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        // arithmetic
        case TOKEN_PLUS:
            emitByte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(OP_SUBTRACT);
            break;
        case TOKEN_STAR:
            emitByte(OP_MULTIPLY);
            break;
        case TOKEN_SLASH:
            emitByte(OP_DIVIDE);
            break;
        // logical
        case TOKEN_EQUAL_EQUAL:
            emitByte(OP_EQUAL);
            break;
        case TOKEN_BANG_EQUAL:
            // a != b is equivalent to !(a == b)
            emitBytes(OP_EQUAL, OP_NOT);
            break;
        case TOKEN_GREATER:
            emitByte(OP_GREATER);
            break;
        case TOKEN_GREATER_EQUAL:
            // a >= b is equivalent to !(a < b)
            emitBytes(OP_LESS, OP_NOT);
            break;
        case TOKEN_LESS:
            emitByte(OP_LESS);
            break;
        case TOKEN_LESS_EQUAL:
            // a <= b is equivalent to !(a > b)
            emitBytes(OP_GREATER, OP_NOT);
        default:
            // unreachable
            return;
    }
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    // compile the operand
    parsePrecedence(PREC_UNARY);

    // emit the operator instruction
    switch (operatorType) {
        case TOKEN_BANG:
            emitByte(OP_NOT);
            break;
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;
        default:
            // unreachable
            return;
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t arg = identifierConstant(&name);

    if (canAssign && match(TOKEN_EQUAL)) {
        // if there is an equal sign, the variable is to be set, not get
        expression();
        emitBytes(OP_SET_GLOBAL, arg);
    } else {
        emitBytes(OP_GET_GLOBAL, arg);
    }
}

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
        default:
            // unreachable
            return;
    }
}

static void string(bool canAssign) {
    // +1 and -2 trim the surrounding quotation marks
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void parsePrecedence(Precedence precedence) {
    advance();

    // prefix expressions (the first token always belongs to a prefix expression)
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    // consume = only if the precedence is no greater than assignment
    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    // infix expressions
    // consume the operator if its precedence is higher than the prefix operator
    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

static void emitReturn() {
    emitByte(OP_RETURN);
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

// Returns the index where the value is added.
static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);

    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static Chunk* currentChunk() {
    return compilingChunk;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) {
        return;
    }
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) {
            return;
        }
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ; // do nothing
        }

        advance();
    }
}