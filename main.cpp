#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include "semantic.hpp"
#include "irgen.hpp"
#include "iropt.hpp"
#include "cfg.hpp"
#include "bytecode.hpp"
#include "irvm.hpp"
#include <unordered_set>
#include <set>
#include <filesystem>

std::unordered_set<std::string> g_class_names;

// Track imported files to prevent circular imports
static std::set<std::string> imported_files;

// Parse a file and return its AST
std::vector<std::unique_ptr<Statement>> parseFile(const std::string &filepath)
{
    // Normalize path
    std::filesystem::path normalized = std::filesystem::absolute(filepath);
    std::string normalizedStr = normalized.string();

    // Check for circular imports
    if (imported_files.count(normalizedStr))
    {
        return {}; // Already imported, skip
    }
    imported_files.insert(normalizedStr);

    // Read and parse file
    std::ifstream file(filepath);
    if (!file.is_open())
    {
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
    const std::string &base_dir)
{
    std::vector<std::unique_ptr<Statement>> result;

    for (auto &stmt : statements)
    {
        if (auto import = dynamic_cast<ImportStatement *>(stmt.get()))
        {
            // Construct full path relative to base_dir
            std::filesystem::path full_path = std::filesystem::path(base_dir) / import->filename;

            // Recursively parse imported file
            auto imported = parseFile(full_path.string());

            // Extract directory from imported file for nested imports
            std::string imported_dir = full_path.parent_path().string();

            // Process imports in imported file
            imported = processImports(std::move(imported), imported_dir);

            // Append to result
            for (auto &s : imported)
            {
                result.push_back(std::move(s));
            }
        }
        else
        {
            result.push_back(std::move(stmt));
        }
    }

    return result;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: tinylang <file.tl|file.tlc> [--compile] [--dump-ir] [--dump-cfg]\n";
        return 1;
    }
    std::string filepath = argv[1];

    // Extract directory from filepath for relative imports
    std::filesystem::path path(filepath);
    std::string base_dir = path.parent_path().string();
    if (base_dir.empty())
    {
        base_dir = ".";
    }

    // Flag helpers (evaluated before try so we can use them everywhere)
    auto hasFlag = [&](const char* flag) {
        for (int i = 2; i < argc; ++i)
            if (std::string(argv[i]) == flag) return true;
        return false;
    };

    // Determine if input is a pre-compiled .tlc file
    bool isBytecode = filepath.size() > 4 &&
                      filepath.compare(filepath.size() - 4, 4, ".tlc") == 0;

    try
    {
        IRProgram ir;

        if (isBytecode) {
            // ── Run pre-compiled bytecode directly ───────────────────────────
            ir = readBytecode(filepath);
        } else {
            // ── Compile from source ──────────────────────────────────────────
            imported_files.clear();

            std::filesystem::path normalized = std::filesystem::absolute(filepath);
            imported_files.insert(normalized.string());

            std::ifstream file(filepath);
            if (!file.is_open()) { std::cerr << "Failed to open file\n"; return 1; }

            std::stringstream buffer;
            buffer << file.rdbuf();
            auto source = buffer.str();

            auto tokens     = tokenize(source);
            auto statements = parse(tokens);
            statements      = processImports(std::move(statements), base_dir);

            semanticAnalyze(statements);
            ir = generateIR(statements);

            // Optimisation pipeline: flat passes → CFG-based passes
            ir = runOptimizationPasses(ir);

            // --compile: write .tlc and exit without running
            if (hasFlag("--compile")) {
                std::string out = filepath.substr(0,
                    filepath.rfind('.') != std::string::npos
                        ? filepath.rfind('.') : filepath.size()) + ".tlc";
                if (writeBytecode(ir, out))
                    std::cerr << "Compiled to " << out << "\n";
                else
                    std::cerr << "Failed to write " << out << "\n";
                return 0;
            }
        }

        // --dump-ir: print optimised flat IR
        if (hasFlag("--dump-ir") || hasFlag("-ir")) dumpIR(ir);

        // --dump-cfg: print CFG with liveness + dominator info
        if (hasFlag("--dump-cfg")) {
            auto dumpOneCFG = [](const std::string& name, const std::vector<IRInstr>& code) {
                CFG cfg = CFG::build(name, code);
                cfg.computeLiveness();
                cfg.computeDominators();
                cfg.computeDomFrontiers();
                cfg.dump();
            };
            dumpOneCFG("[main]", ir.main);
            for (auto& [key, fn] : ir.functions)
                dumpOneCFG(key, fn.code);
        }

        runIR(ir);
    }
    catch (std::exception &e)
    {
        std::cerr << "Compiler error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
