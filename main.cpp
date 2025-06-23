#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include <unordered_set>

std::unordered_set<std::string> g_class_names;

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
        // Debug: Print top-level AST statement types
        // std::cout << "[DEBUG][Main] Top-level AST statements:" << std::endl;
        // for (const auto& stmt : ast) {
        //     if (dynamic_cast<ClassDef*>(stmt.get())) {
        //         std::cout << "  ClassDef" << std::endl;
        //     } else if (auto assign = dynamic_cast<Assignment*>(stmt.get())) {
        //         std::cout << "  Assignment: name=" << assign->name << ", type=" << assign->type << std::endl;
        //     } else if (dynamic_cast<Print*>(stmt.get())) {
        //         std::cout << "  Print" << std::endl;
        //     } else if (dynamic_cast<ExprStatement*>(stmt.get())) {
        //         std::cout << "  ExprStatement" << std::endl;
        //     } else if (dynamic_cast<FunctionDef*>(stmt.get())) {
        //         std::cout << "  FunctionDef" << std::endl;
        //     } else {
        //         std::cout << "  Other statement" << std::endl;
        //     }
        // }
        run(ast);
    } catch (std::exception& e) {
        std::cerr << "Compiler error: " << e.what() << std::endl;
    }

    return 0;
}
