#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
void run(const std::vector<std::unique_ptr<Statement>>& statements);  // Forward declaration

int main() {
    std::ifstream file("sample.tl");
    if (!file.is_open()) {
        std::cerr << "Failed to open file\n";
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    auto source = buffer.str();

    try {
        auto tokens = tokenize(source);
        auto ast = parse(tokens);
        run(ast);
    } catch (std::exception& e) {
        std::cerr << "Compiler error: " << e.what() << std::endl;
    }

    return 0;
}
