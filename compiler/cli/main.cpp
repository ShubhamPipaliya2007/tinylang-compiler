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
                     "[--compile] [--dump-ir] [--dump-cfg] [--old-ir]\n";
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
