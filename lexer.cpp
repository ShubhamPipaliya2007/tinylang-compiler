#include "lexer.hpp"
#include <cctype>
#include <iostream>
#include <stdexcept>

std::vector<Token> tokenize(const std::string& input) {
    std::vector<Token> tokens;
    size_t i = 0;

    while (i < input.length()) {
        char c = input[i];

        if (std::isspace(c)) {
            i++;
        }
        else if (std::isalpha(c)) {
            std::string id;
            while (i < input.length() && (std::isalnum(input[i]) || input[i] == '_')) {
                id += input[i++];
            }

            if (id == "if") tokens.push_back({TokenType::IF, id});
            else if (id == "else") tokens.push_back({TokenType::ELSE, id});
            else if (id == "return") tokens.push_back({TokenType::RETURN, id});
            else if (id == "print") tokens.push_back({TokenType::PRINT, id});
            else if (id == "int") tokens.push_back({TokenType::INT, id});
            else if (id == "ComeAndDo") tokens.push_back({TokenType::COMEANDDO, id});
            else if (id == "while") tokens.push_back({TokenType::WHILE, id});
            else if (id == "for") tokens.push_back({TokenType::FOR, id});
            else if (id == "bool") tokens.push_back({TokenType::BOOL, id});
            else if (id == "string") tokens.push_back({TokenType::STRING_TYPE, id}); 
            else if (id == "read") tokens.push_back({TokenType::READ, id});
            else if (id == "input") tokens.push_back({TokenType::INPUT, id});
            else if (id == "true" || id == "false") tokens.push_back({TokenType::BOOLEAN_LITERAL, id});
            else tokens.push_back({TokenType::IDENTIFIER, id});
        }
        else if (std::isdigit(c)) {
            std::string num;
            while (i < input.length() && std::isdigit(input[i])) {
                num += input[i++];
            }
            tokens.push_back({TokenType::NUMBER, num});
        }
        else if (c == '"') {
            i++;  // skip opening quote
            std::string str;
            while (i < input.length() && input[i] != '"') {
                str += input[i++];
            }
            if (i >= input.length()) {
                throw std::runtime_error("Unterminated string literal");
            }
            i++; // skip closing quote
            tokens.push_back({TokenType::STRING_LITERAL, str});
        } else {
            switch (c) {
                case '+': tokens.push_back({TokenType::PLUS, "+"}); break;
                case '-': tokens.push_back({TokenType::MINUS, "-"}); break;
                case '*': tokens.push_back({TokenType::MULTIPLICATION, "*"}); break;
                case '/': tokens.push_back({TokenType::DIVISION, "/"}); break;
                case ';': tokens.push_back({TokenType::SEMICOLON, ";"}); break;
                case '(': tokens.push_back({TokenType::LPAREN, "("}); break;
                case ')': tokens.push_back({TokenType::RPAREN, ")"}); break;
                case '{': tokens.push_back({TokenType::LBRACE, "{"}); break;
                case '}': tokens.push_back({TokenType::RBRACE, "}"}); break;
                case ',': tokens.push_back({TokenType::COMMA, ","}); break;
                case '>': tokens.push_back({TokenType::GREATERTHEN, ">"}); break;
                case '<': tokens.push_back({TokenType::LESSTHEN, "<"}); break;
                case '=':
                    if (i + 1 < input.length() && input[i + 1] == '=') {
                        tokens.push_back({TokenType::EQUALTO, "=="});
                        i++;
                    } else {
                        tokens.push_back({TokenType::ASSIGN, "="});
                    }
                    break;
                case '!':
                    if (i + 1 < input.length() && input[i + 1] == '=') {
                        tokens.push_back({TokenType::NOTEQUALTO, "!="});
                        i++;
                    } else {
                        throw std::runtime_error("Unexpected token: !");
                    }
                    break;
                default:
                    std::cerr << "Unknown character: " << c << "\n";
                    break;
            }
            i++;
        }
    }

    tokens.push_back({TokenType::END, ""});
    return tokens;
}
