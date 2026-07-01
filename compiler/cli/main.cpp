#include <iostream>
#include <fstream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "ast.hpp"
#include "semantic.hpp"
// Legacy IR pipeline (optimizer, bytecode, CFG analysis, old VM)
#include "irgen.hpp"
#include "iropt.hpp"
#include "cfg.hpp"
#include "bytecode.hpp"
#include "irvm.hpp"
// New register-based TIR pipeline
#include "tirgen.hpp"
#include "tirvm.hpp"
// LLVM backend
#include "llvmgen.hpp"
#include <unordered_set>
#include <set>
#include <filesystem>

std::unordered_set<std::string> g_class_names;

static std::set<std::string> imported_files;

std::vector<std::unique_ptr<Statement>> parseFile(const std::string& filepath) {
    std::filesystem::path normalized = std::filesystem::absolute(filepath);
    std::string normalizedStr = normalized.string();
    if (imported_files.count(normalizedStr)) return {};
    imported_files.insert(normalizedStr);

    std::ifstream file(filepath);
    if (!file.is_open())
        throw std::runtime_error("Failed to open imported file: " + filepath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    auto tokens = tokenize(buffer.str());
    return parse(tokens);
}

// Walk imported files (transitively) and register every class name found.
// This must run before parse() so that object-instantiation syntax
// ("ClassName varName(args)") is recognized in the main file.
static void prescanForClassNames(const std::vector<Token>& tokens,
                                  const std::string& base_dir) {
    for (size_t i = 0; i < tokens.size(); ++i) {
        // Register class names defined in this file.
        if (tokens[i].type == TokenType::CLASS &&
            i + 1 < tokens.size() &&
            tokens[i + 1].type == TokenType::IDENTIFIER) {
            g_class_names.insert(tokens[i + 1].value);
        }
        // Recurse into imported files.
        if (tokens[i].type == TokenType::IMPORT &&
            i + 1 < tokens.size() &&
            tokens[i + 1].type == TokenType::STRING_LITERAL) {
            std::string fname = tokens[i + 1].value;
            std::filesystem::path full = std::filesystem::path(base_dir) / fname;
            std::ifstream f(full.string());
            if (f.is_open()) {
                std::stringstream buf; buf << f.rdbuf();
                auto itoks = tokenize(buf.str());
                prescanForClassNames(itoks, full.parent_path().string());
            }
        }
    }
}

std::vector<std::unique_ptr<Statement>> processImports(
    std::vector<std::unique_ptr<Statement>> statements,
    const std::string& base_dir)
{
    std::vector<std::unique_ptr<Statement>> result;
    for (auto& stmt : statements) {
        if (auto imp = dynamic_cast<ImportStatement*>(stmt.get())) {
            std::filesystem::path full = std::filesystem::path(base_dir) / imp->filename;
            auto imported = parseFile(full.string());
            imported = processImports(std::move(imported),
                                      full.parent_path().string());
            for (auto& s : imported) result.push_back(std::move(s));
        } else {
            result.push_back(std::move(stmt));
        }
    }
    return result;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: tinylang <file.tl|file.tlc> "
                     "[--compile] [--dump-ir] [--dump-cfg] [--old-ir] "
                     "[--emit-llvm [out.ll]]\n";
        return 1;
    }
    std::string filepath = argv[1];
    std::filesystem::path path(filepath);
    std::string base_dir = path.parent_path().string();
    if (base_dir.empty()) base_dir = ".";

    auto hasFlag = [&](const char* flag) {
        for (int i = 2; i < argc; ++i)
            if (std::string(argv[i]) == flag) return true;
        return false;
    };
    // Return the argument after 'flag', or "" if flag not found / no argument.
    auto getFlagArg = [&](const char* flag) -> std::string {
        for (int i = 2; i + 1 < argc; ++i)
            if (std::string(argv[i]) == flag) return argv[i + 1];
        return "";
    };

    bool isBytecode = filepath.size() > 4 &&
                      filepath.compare(filepath.size()-4, 4, ".tlc") == 0;

    try {
        // ── Pre-compiled bytecode: use legacy VM ──────────────────────────
        if (isBytecode) {
            IRProgram ir = readBytecode(filepath);
            if (hasFlag("--dump-ir") || hasFlag("-ir")) dumpIR(ir);
            if (hasFlag("--dump-cfg")) {
                auto dumpOne = [](const std::string& name,
                                  const std::vector<IRInstr>& code) {
                    CFG cfg = CFG::build(name, code);
                    cfg.computeLiveness();
                    cfg.computeDominators();
                    cfg.computeDomFrontiers();
                    cfg.dump();
                };
                dumpOne("[main]", ir.main);
                for (auto& [k, fn] : ir.functions) dumpOne(k, fn.code);
            }
            runIR(ir);
            return 0;
        }

        // ── Source file: parse + semantic ─────────────────────────────────
        imported_files.clear();
        std::filesystem::path normalized = std::filesystem::absolute(filepath);
        imported_files.insert(normalized.string());

        std::ifstream file(filepath);
        if (!file.is_open()) { std::cerr << "Failed to open file\n"; return 1; }
        std::stringstream buffer;
        buffer << file.rdbuf();

        auto tokens     = tokenize(buffer.str());
        prescanForClassNames(tokens, base_dir);  // populate g_class_names before parse()
        auto statements = parse(tokens);
        statements      = processImports(std::move(statements), base_dir);
        semanticAnalyze(statements);

        // ── Legacy path: --compile, --dump-cfg, or --old-ir ──────────────
        bool needOldIR = hasFlag("--compile") || hasFlag("--dump-cfg") || hasFlag("--old-ir");
        IRProgram ir;
        if (needOldIR) {
            ir = generateIR(statements);
            ir = runOptimizationPasses(ir);
        }

        // --compile: write .tlc bytecode using legacy IR, then exit
        if (hasFlag("--compile")) {
            std::string out = filepath.substr(
                0, filepath.rfind('.') != std::string::npos
                   ? filepath.rfind('.') : filepath.size()) + ".tlc";
            if (writeBytecode(ir, out))
                std::cerr << "Compiled to " << out << "\n";
            else
                std::cerr << "Failed to write " << out << "\n";
            return 0;
        }

        // --old-ir: execute with legacy stack-based VM
        if (hasFlag("--old-ir")) {
            if (hasFlag("--dump-ir") || hasFlag("-ir")) dumpIR(ir);
            if (hasFlag("--dump-cfg")) {
                auto dumpOne = [](const std::string& name,
                                  const std::vector<IRInstr>& code) {
                    CFG cfg = CFG::build(name, code);
                    cfg.computeLiveness();
                    cfg.computeDominators();
                    cfg.computeDomFrontiers();
                    cfg.dump();
                };
                dumpOne("[main]", ir.main);
                for (auto& [k, fn] : ir.functions) dumpOne(k, fn.code);
            }
            runIR(ir);
            return 0;
        }

        // ── Default: generate TIR and execute with TIRVM ──────────────────
        TIR::Program tir = generateTIR(statements);

        if (hasFlag("--dump-ir") || hasFlag("-ir")) dumpTIR(tir);

        // --emit-llvm [output.ll]: write LLVM IR and exit (no execution).
        if (hasFlag("--emit-llvm")) {
            std::string llvmOut = getFlagArg("--emit-llvm");
            if (llvmOut.empty() || llvmOut[0] == '-') {
                // No output path given: derive from input filename.
                std::string base = filepath.substr(
                    0, filepath.rfind('.') != std::string::npos
                       ? filepath.rfind('.') : filepath.size());
                llvmOut = base + ".ll";
            }
            std::string llvmIR = emitLLVM(tir);
            std::ofstream out(llvmOut);
            if (!out.is_open()) {
                std::cerr << "Failed to write " << llvmOut << "\n";
                return 1;
            }
            out << llvmIR;
            std::cerr << "LLVM IR written to " << llvmOut << "\n";
            std::cerr << "To compile: clang " << llvmOut
                      << " runtime/native/tinyrt.c -o program\n";
            return 0;
        }

        if (hasFlag("--dump-cfg")) {
            // CFG analysis still works on legacy IR; generate it for this flag.
            auto dumpOne = [](const std::string& name,
                              const std::vector<IRInstr>& code) {
                CFG cfg = CFG::build(name, code);
                cfg.computeLiveness();
                cfg.computeDominators();
                cfg.computeDomFrontiers();
                cfg.dump();
            };
            dumpOne("[main]", ir.main);
            for (auto& [k, fn] : ir.functions) dumpOne(k, fn.code);
        }

        runTIR(tir);
    }
    catch (std::exception& e) {
        std::cerr << "Compiler error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
