#pragma once
#include "ast.hpp"
#include <string>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Diagnostic types
// ---------------------------------------------------------------------------

struct SemanticError {
    std::string message;
    int line = 0;
};

struct SemanticWarning {
    std::string message;
    int line = 0;
};

// ---------------------------------------------------------------------------
// Scoped symbol table
// ---------------------------------------------------------------------------

class SymbolTable {
public:
    struct Symbol {
        std::string name;
        std::string type;
        bool initialized = true; // false when declared without a value
    };

    SymbolTable() { scopes_.push_back({}); }

    void pushScope() { scopes_.push_back({}); }
    void popScope()  { if (scopes_.size() > 1) scopes_.pop_back(); }

    // Returns false if already declared in the current (innermost) scope.
    bool declare(const std::string& name, const std::string& type, bool initialized = true) {
        auto& top = scopes_.back();
        if (top.count(name)) return false;
        top[name] = {name, type, initialized};
        return true;
    }

    // Walk scopes from innermost outward; returns nullptr if not found.
    const Symbol* lookup(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return &found->second;
        }
        return nullptr;
    }

    // Returns true if the name exists in any scope OUTSIDE the innermost.
    bool existsInOuter(const std::string& name) const {
        if (scopes_.size() < 2) return false;
        for (size_t i = 0; i + 1 < scopes_.size(); ++i) {
            if (scopes_[i].count(name)) return true;
        }
        return false;
    }

    bool existsInCurrentScope(const std::string& name) const {
        return scopes_.back().count(name) > 0;
    }

    // Mark a previously declared variable as initialized (after reassignment).
    void setInitialized(const std::string& name) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) { found->second.initialized = true; return; }
        }
    }

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes_;
};

// ---------------------------------------------------------------------------
// Function and class metadata
// ---------------------------------------------------------------------------

struct FunctionInfo {
    std::string returnType = "void";
    std::vector<std::pair<std::string, std::string>> params; // (type, name)
    bool hasValueReturn = false; // true if any `return expr;` exists in the body
};

struct ClassInfo {
    std::string baseClass;
    std::unordered_map<std::string, std::string> fieldTypes; // field name -> type
    std::unordered_map<std::string, FunctionInfo> methods;
};

// ---------------------------------------------------------------------------
// Semantic analyzer
// ---------------------------------------------------------------------------

class SemanticAnalyzer {
public:
    // Folds constants in-place, then runs all analysis passes.
    // Returns accumulated errors; warnings are available via warnings().
    std::vector<SemanticError> analyze(std::vector<std::unique_ptr<Statement>>& stmts);

    const std::vector<SemanticWarning>& warnings() const { return warnings_; }

private:
    SymbolTable symbols_;
    std::unordered_map<std::string, FunctionInfo> functions_;
    std::unordered_map<std::string, ClassInfo>    classes_;
    std::vector<SemanticError>   errors_;
    std::vector<SemanticWarning> warnings_;
    std::string currentClass_;
    std::string currentFunction_;

    void error(const std::string& msg, int line = 0);
    void warn (const std::string& msg, int line = 0);

    // Pass 1: register all top-level functions and classes.
    void firstPass(const std::vector<std::unique_ptr<Statement>>& stmts);

    // Analyze a list of statements; returns true if a return is guaranteed.
    // Also flags unreachable code (statements after a guaranteed return).
    bool analyzeStatementList(const std::vector<std::unique_ptr<Statement>>& stmts);

    void analyzeStatement(const Statement* stmt);

    // Returns the inferred type string, or "unknown" when unresolvable.
    std::string analyzeExpr(const Expr* expr);

    // True if all code paths through `stmts` end with a return statement.
    bool guaranteedReturn(const std::vector<std::unique_ptr<Statement>>& stmts) const;

    // Looks up a class by name.
    const ClassInfo* resolveClass(const std::string& name) const;

    // Returns true if `actual` is implicitly assignable to `declared`.
    bool typesCompatible(const std::string& declared, const std::string& actual) const;

    // Returns true if an explicit cast from `from` to `to` is allowed.
    bool castAllowed(const std::string& to, const std::string& from) const;

    // Walk the inheritance chain to find a field type; "" if not found.
    std::string findFieldType(const std::string& className, const std::string& field) const;

    // Walk the inheritance chain to find a method; nullptr if not found.
    const FunctionInfo* findMethod(const std::string& className, const std::string& method) const;
};

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

// Folds constant expressions, runs semantic analysis, prints all diagnostics.
// Throws std::runtime_error if there are any errors (warnings are non-fatal).
void semanticAnalyze(std::vector<std::unique_ptr<Statement>>& stmts);
