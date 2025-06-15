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
    LBRACE,
    RBRACE,
    COMMA,
    RETURN,
    COMEANDDO,
    IF,
    ELSE,
    GREATERTHEN,
    LESSTHEN,
    EQUALTO,
    NOTEQUALTO,
    END
};



struct Token {
    TokenType type;
    std::string value;
};

std::vector<Token> tokenize(const std::string& input);
