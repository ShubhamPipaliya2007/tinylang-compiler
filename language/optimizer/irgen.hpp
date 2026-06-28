#pragma once
#include "ir.hpp"
#include "ast.hpp"
#include <unordered_set>

// ---------------------------------------------------------------------------
// IRGen – walks the (already semantically-analysed) AST and emits IR
// ---------------------------------------------------------------------------
class IRGen {
public:
    // Generate an IRProgram from the top-level statement list.
    IRProgram generate(const std::vector<std::unique_ptr<Statement>>& stmts);

    // Print the IR to stdout in a human-readable format.
    static void dump(const IRProgram& prog);

private:
    IRProgram             prog_;
    std::vector<IRInstr>* cur_ = nullptr; // current emission target
    int                   labelCount_ = 0;

    // Per-function context used when compiling a class method
    std::string                                    currentClass_;
    std::unordered_set<std::string>                currentParams_;
    std::vector<std::pair<std::string,std::string>> currentFields_; // all fields incl. inherited

    // ---- helpers -------------------------------------------------------
    std::string newLabel(const std::string& pfx = "L");
    void emit(IROp op, const std::string& s = "", int i = 0,
              double d = 0.0, char c = 0);

    // ---- passes --------------------------------------------------------
    // First pass: collect class metadata and compile all functions/methods.
    void firstPass(const std::vector<std::unique_ptr<Statement>>& stmts);

    // Compile a single FunctionDef (possibly a class method) into prog_.functions.
    void compileFunction(const FunctionDef* func, const std::string& cls = "");

    // Recurse through the class hierarchy to collect all fields (base first).
    void collectAllFields(const std::string& className,
                          std::vector<std::pair<std::string,std::string>>& out) const;

    // ---- code generation -----------------------------------------------
    void genStmt(const Statement* stmt);
    void genExpr(const Expr* expr);
};

// ---------------------------------------------------------------------------
// Convenience wrappers (used by main.cpp)
// ---------------------------------------------------------------------------
IRProgram generateIR(const std::vector<std::unique_ptr<Statement>>& stmts);
void      dumpIR(const IRProgram& prog);
