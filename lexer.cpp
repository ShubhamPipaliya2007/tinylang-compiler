#include "lexer.hpp"
#include <cctype>
#include <iostream>
#include <stdexcept>

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    size_t i = 0;
    int line = 1;
    int col = 1;

    while (i < input.length()) {
        char c = input[i];
        int token_line = line;
        int token_col = col;

        if (c == '\n') {
            i++;
            line++;
            col = 1;
            continue;
        }
        if (std::isspace(c)) {
            i++;
            col++;
        }
        else if (std::isalpha(c)) {
            std::string id;
            size_t start_i = i;
            int start_col = col;
            while (i < input.length() && (std::isalnum(input[i]) || input[i] == '_')) {
                id += input[i++];
                col++;
            }
            TokenType type;
            if (id == "if") type = TokenType::IF;
            else if (id == "else") type = TokenType::ELSE;
            else if (id == "return") type = TokenType::RETURN;
            else if (id == "print") type = TokenType::PRINT;
            else if (id == "int") type = TokenType::INT;
            else if (id == "float") type = TokenType::FLOAT;
            else if (id == "char") type = TokenType::CHAR;
            else if (id == "ComeAndDo") type = TokenType::COMEANDDO;
            else if (id == "while") type = TokenType::WHILE;
            else if (id == "for") type = TokenType::FOR;
            else if (id == "bool") type = TokenType::BOOL;
            else if (id == "string") type = TokenType::STRING_TYPE;
            else if (id == "read") type = TokenType::READ;
            else if (id == "input") type = TokenType::INPUT;
            else if (id == "true" || id == "false") type = TokenType::BOOLEAN_LITERAL;
            else type = TokenType::IDENTIFIER;
            tokens.push_back({type, id, token_line, token_col});
        }
        else if (std::isdigit(c)) {
            std::string num;
            bool is_float = false;
            while (i < input.length() && std::isdigit(input[i])) {
                num += input[i++];
                col++;
            }
            if (i < input.length() && input[i] == '.') {
                is_float = true;
                num += input[i++];
                col++;
                while (i < input.length() && std::isdigit(input[i])) {
                    num += input[i++];
                    col++;
                }
            }
            if (is_float) {
                tokens.push_back({TokenType::FLOAT_LITERAL, num, token_line, token_col});
            } else {
                tokens.push_back({TokenType::NUMBER, num, token_line, token_col});
            }
        }
        else if (c == '"') {
            i++; col++;
            std::string str;
            int str_start_col = col;
            while (i < input.length() && input[i] != '"') {
                if (input[i] == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
                str += input[i++];
            }
            if (i >= input.length()) {
                throw std::runtime_error("Unterminated string literal at line " + std::to_string(token_line) + ", column " + std::to_string(token_col));
            }
            i++; col++;
            tokens.push_back({TokenType::STRING_LITERAL, str, token_line, token_col});
        }
        else if (c == '\'') {
            i++; col++;
            if (i < input.length() && input[i + 1] == '\'') {
                std::string ch(1, input[i]);
                tokens.push_back({TokenType::CHAR_LITERAL, ch, token_line, token_col});
                i += 2; col += 2;
            } else {
                throw std::runtime_error("Unterminated or invalid char literal at line " + std::to_string(token_line) + ", column " + std::to_string(token_col));
            }
        }
        else {
            TokenType type;
            std::string val;
            bool valid = true;
            switch (c) {
                case '+': type = TokenType::PLUS; val = "+"; break;
                case '-': type = TokenType::MINUS; val = "-"; break;
                case '*': type = TokenType::MULTIPLICATION; val = "*"; break;
                case '/': type = TokenType::DIVISION; val = "/"; break;
                case ';': type = TokenType::SEMICOLON; val = ";"; break;
                case '(': type = TokenType::LPAREN; val = "("; break;
                case ')': type = TokenType::RPAREN; val = ")"; break;
                case '{': type = TokenType::LBRACE; val = "{"; break;
                case '}': type = TokenType::RBRACE; val = "}"; break;
                case '[': type = TokenType::LBRACKET; val = "["; break;
                case ']': type = TokenType::RBRACKET; val = "]"; break;
                case ',': type = TokenType::COMMA; val = ","; break;
                case '>': type = TokenType::GREATERTHEN; val = ">"; break;
                case '<': type = TokenType::LESSTHEN; val = "<"; break;
                case '=':
                    if (i + 1 < input.length() && input[i + 1] == '=') {
                        type = TokenType::EQUALTO; val = "==";
                        i++; col++;
                    } else {
                        type = TokenType::ASSIGN; val = "=";
                    }
                    break;
                case '!':
                    if (i + 1 < input.length() && input[i + 1] == '=') {
                        type = TokenType::NOTEQUALTO; val = "!=";
                        i++; col++;
                    } else {
                        throw std::runtime_error("Unexpected token: ! at line " + std::to_string(token_line) + ", column " + std::to_string(token_col));
                    }
                    break;
                default:
                    std::cerr << "Unknown character: " << c << " at line " << line << ", column " << col << "\n";
                    valid = false;
                    break;
            }
            if (valid) {
                tokens.push_back({type, val, token_line, token_col});
            }
            i++; col++;
        }
    }

    tokens.push_back({TokenType::END, "", line, col});
    return tokens;
}
