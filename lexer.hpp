#pragma once
#include <string>
#include <vector>

enum class TokenType {
    INT,
    IDENTIFIER,
    NUMBER,
    ASSIGN,
    PLUS,
    MINUS,
    MULTIPLICATION,
    DIVISION,
    SEMICOLON,
    PRINT,
    LPAREN,
    RPAREN,
    LBRACE,    // {
    RBRACE,    // }
    LBRACKET,  // [
    RBRACKET,  // ]
    COMMA,
    RETURN,
    COMEANDDO,
    IF,
    ELSE,
    GREATERTHEN,
    LESSTHEN,
    EQUALTO,
    NOTEQUALTO,
    END,
    FOR,
    WHILE,
    BOOL,
    BOOLEAN_LITERAL,
    STRING_TYPE,
    STRING_LITERAL,
    INPUT,
    READ,
    FLOAT,           // float keyword
    CHAR,            // char keyword
    FLOAT_LITERAL,   // float literal
    CHAR_LITERAL,    // char literal
    AND,             // &&
    OR,              // ||
    NOT,             // !
    CLASS,           // class keyword
    DOT,             // . operator for member access
    COLON,            // : for inheritance
};


struct Token {
    TokenType type;
    std::string value;
    int line;   // Line number (1-based)
    int column; // Column number (1-based)
};

std::vector<Token> tokenize(const std::string& input);
