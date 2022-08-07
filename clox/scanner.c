#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

static Token number(Scanner* scanner);
static Token string(Scanner* scanner);
static Token makeToken(Scanner* scanner, TokenType type);
static Token errorToken(Scanner* scanner, const char* message);
static void skipWhitespace(Scanner* scanner);
static char advance(Scanner* scanner);
static bool match(Scanner* scanner, char expected);
static char peek(Scanner* scanner);
static char peekNext(Scanner* scanner);
static bool isAtEnd(Scanner* scanner);
static bool isDigit(char c);

void initScanner(Scanner* scanner, const char* source) {
    scanner->start = source;
    scanner->current = source;
    scanner->line = 1;
}

Token scanToken(Scanner* scanner) {
    skipWhitespace(scanner);
    scanner->start = scanner->current;

    if (isAtEnd(scanner)) {
        return makeToken(scanner, TOKEN_EOF);
    }

    char c = advance(scanner);

    if (isDigit(c)) {
        return number(scanner);
    }

    switch(c) {
        case '(':
            return makeToken(scanner, TOKEN_LEFT_PAREN);
        case ')':
            return makeToken(scanner, TOKEN_RIGHT_PAREN);
        case '{':
            return makeToken(scanner, TOKEN_LEFT_BRACE);
        case '}':
            return makeToken(scanner, TOKEN_RIGHT_BRACE);
        case ';':
            return makeToken(scanner, TOKEN_SEMICOLON);
        case ',':
            return makeToken(scanner, TOKEN_COMMA);
        case '.':
            return makeToken(scanner, TOKEN_DOT);
        case '-':
            return makeToken(scanner, TOKEN_MINUS);
        case '+':
            return makeToken(scanner, TOKEN_PLUS);
        case '/':
            return makeToken(scanner, TOKEN_SLASH);
        case '*':
            return makeToken(scanner, TOKEN_STAR);
        case '!':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(scanner, match(scanner, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"':
            return string(scanner);
    }

    return errorToken(scanner, "Unexpected character");
}

static Token number(Scanner* scanner) {
    while (isDigit(peek(scanner))) {
        advance(scanner);
    }

    // look for a fractional part
    if (peek(scanner) == '.' && isDigit(peekNext(scanner))) {
        // consume the dot
        advance(scanner);

        // consume the fractional part
        while (isDigit(peek(scanner))) {
            advance(scanner);
        }
    }

    return makeToken(scanner, TOKEN_NUMBER);
}

static Token string(Scanner* scanner) {
    while (peek(scanner) != '"' && !isAtEnd(scanner)) {
        if (peek(scanner) == '\n') {
            scanner->line++;
        }
        advance(scanner);
    }

    if (isAtEnd) {
        return errorToken(scanner, "Unterminated string.");
    }

    // consume the closing quote
    advance(scanner);

    return makeToken(scanner, TOKEN_STRING);
}

static Token makeToken(Scanner* scanner, TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner->start;
    token.length = (int)(scanner->current - scanner->start);
    token.line = scanner->line;

    return token;
}

static Token errorToken(Scanner* scanner, const char* message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner->line;

    return token;
}

static void skipWhitespace(Scanner* scanner) {
    for (;;) {
        char c = peek(scanner);
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance(scanner);
                break;
            case '\n':
                scanner->line++;
                advance(scanner);
                break;
            case '/':
                if (peekNext(scanner) == '/') {
                    // a comment goes until the end of the line.
                    while (peek(scanner) != '\n' && !isAtEnd(scanner)) {
                        advance(scanner);
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static char advance(Scanner* scanner) {
    scanner->current++;
    return scanner->current[-1];
}

static bool match(Scanner* scanner, char expected) {
    if (isAtEnd(scanner) || scanner->current != expected) {
        return false;
    }
    scanner->current++;
    return true;
}

static char peek(Scanner* scanner) {
    return scanner->current;
}

static char peekNext(Scanner* scanner) {
    if (isAtEnd(scanner)) {
        return '\0';
    }

    return scanner->current[1];
}

static bool isAtEnd(Scanner* scanner) {
    return scanner->current == '\0';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}
