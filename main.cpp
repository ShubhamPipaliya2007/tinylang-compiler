#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include <unordered_set>
#include <set>
#include <filesystem>

std::unordered_set<std::string> g_class_names;

void run(const std::vector<std::unique_ptr<Statement>>& statements);  // Forward declaration

// Track imported files to prevent circular imports
static std::set<std::string> imported_files;

// Parse a file and return its AST
std::vector<std::unique_ptr<Statement>> parseFile(const std::string& filepath) {
    // Normalize path
    std::filesystem::path normalized = std::filesystem::absolute(filepath);
    std::string normalizedStr = normalized.string();

    // Check for circular imports
    if (imported_files.count(normalizedStr)) {
        return {}; // Already imported, skip
    }
    imported_files.insert(normalizedStr);

    // Read and parse file
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open imported file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    auto source = buffer.str();

    auto tokens = tokenize(source);
    return parse(tokens);
}

// Process import statements recursively
std::vector<std::unique_ptr<Statement>> processImports(
    std::vector<std::unique_ptr<Statement>> statements,
    const std::string& base_dir
) {
    std::vector<std::unique_ptr<Statement>> result;

    for (auto& stmt : statements) {
        if (auto import = dynamic_cast<ImportStatement*>(stmt.get())) {
            // Construct full path relative to base_dir
            std::filesystem::path full_path = std::filesystem::path(base_dir) / import->filename;

            // Recursively parse imported file
            auto imported = parseFile(full_path.string());

            // Extract directory from imported file for nested imports
            std::string imported_dir = full_path.parent_path().string();

            // Process imports in imported file
            imported = processImports(std::move(imported), imported_dir);

            // Append to result
            for (auto& s : imported) {
                result.push_back(std::move(s));
            }
        } else {
            result.push_back(std::move(stmt));
        }
    }

    return result;
}

int main() {
    std::string filepath = "sample.tl";

    // Extract directory from filepath for relative imports
    std::filesystem::path path(filepath);
    std::string base_dir = path.parent_path().string();
    if (base_dir.empty()) {
        base_dir = ".";
    }

    try {
        imported_files.clear();

        // Normalize and track main file path
        std::filesystem::path normalized = std::filesystem::absolute(filepath);
        imported_files.insert(normalized.string());

        // Read main file
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "Failed to open file\n";
            return 1;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        auto source = buffer.str();

        // Parse main file
        auto tokens = tokenize(source);
        auto statements = parse(tokens);

        // Process all imports recursively
        statements = processImports(std::move(statements), base_dir);

        // Execute
        run(statements);
    } catch (std::exception& e) {
        std::cerr << "Compiler error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
