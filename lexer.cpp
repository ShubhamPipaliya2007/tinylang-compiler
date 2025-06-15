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
            else tokens.push_back({TokenType::IDENTIFIER, id});
        }
        else if (std::isdigit(c)) {
            std::string num;
            while (i < input.length() && std::isdigit(input[i])) {
                num += input[i++];
            }
            tokens.push_back({TokenType::NUMBER, num});
        }
        else {
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
