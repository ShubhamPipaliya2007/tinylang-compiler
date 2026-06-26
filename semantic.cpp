#include "semantic.hpp"
#include <iostream>
#include <unordered_set>

// ===========================================================================
// Constant-folding pass  (modifies the AST in-place before analysis)
// ===========================================================================

static void foldExpr(std::unique_ptr<Expr>& expr);

static void foldStmt(Statement* stmt) {
    if (!stmt) return;
    if (auto* a = dynamic_cast<Assignment*>(stmt)) {
        if (a->value) foldExpr(a->value);
    } else if (auto* p = dynamic_cast<Print*>(stmt)) {
        foldExpr(p->value);
    } else if (auto* r = dynamic_cast<Return*>(stmt)) {
        if (r->value) foldExpr(r->value);
    } else if (auto* e = dynamic_cast<ExprStatement*>(stmt)) {
        foldExpr(e->expr);
    } else if (auto* ifs = dynamic_cast<IfStatement*>(stmt)) {
        foldExpr(ifs->condition);
        for (auto& s : ifs->thenBranch) foldStmt(s.get());
        for (auto& s : ifs->elseBranch) foldStmt(s.get());
    } else if (auto* ws = dynamic_cast<WhileStatement*>(stmt)) {
        foldExpr(ws->condition);
        for (auto& s : ws->body) foldStmt(s.get());
    } else if (auto* fs = dynamic_cast<ForStatement*>(stmt)) {
        if (fs->initializer) foldStmt(fs->initializer.get());
        if (fs->condition)   foldExpr(fs->condition);
        if (fs->increment)   foldStmt(fs->increment.get());
        for (auto& s : fs->body) foldStmt(s.get());
    } else if (auto* fn = dynamic_cast<FunctionDef*>(stmt)) {
        for (auto& s : fn->body) foldStmt(s.get());
    } else if (auto* cls = dynamic_cast<ClassDef*>(stmt)) {
        for (auto& m : cls->methods)
            for (auto& s : m->body) foldStmt(s.get());
    } else if (auto* oi = dynamic_cast<ObjectInstantiation*>(stmt)) {
        for (auto& a : oi->arguments) foldExpr(a);
    } else if (auto* aa = dynamic_cast<ArrayAssignment*>(stmt)) {
        foldExpr(aa->index);
        foldExpr(aa->value);
    }
}

static void foldExpr(std::unique_ptr<Expr>& expr) {
    if (!expr) return;

    // Recurse first so children are folded before we try to fold the parent.
    if (auto* bin = dynamic_cast<BinaryExpr*>(expr.get())) {
        foldExpr(bin->left);
        foldExpr(bin->right);

        auto* ln = dynamic_cast<Number*>(bin->left.get());
        auto* rn = dynamic_cast<Number*>(bin->right.get());
        auto* lf = dynamic_cast<FloatLiteral*>(bin->left.get());
        auto* rf = dynamic_cast<FloatLiteral*>(bin->right.get());

        bool lConst = ln || lf;
        bool rConst = rn || rf;
        if (!lConst || !rConst) return;

        bool isFloat = lf || rf;
        double lv = lf ? lf->value : (double)ln->value;
        double rv = rf ? rf->value : (double)rn->value;
        int    li  = ln ? ln->value : (int)lf->value;
        int    ri  = rn ? rn->value : (int)rf->value;

        switch (bin->op) {
            case TokenType::PLUS:
                expr = isFloat ? (std::unique_ptr<Expr>)std::make_unique<FloatLiteral>(lv + rv)
                               : std::make_unique<Number>(li + ri);
                break;
            case TokenType::MINUS:
                expr = isFloat ? (std::unique_ptr<Expr>)std::make_unique<FloatLiteral>(lv - rv)
                               : std::make_unique<Number>(li - ri);
                break;
            case TokenType::MULTIPLICATION:
                expr = isFloat ? (std::unique_ptr<Expr>)std::make_unique<FloatLiteral>(lv * rv)
                               : std::make_unique<Number>(li * ri);
                break;
            case TokenType::DIVISION:
                if (rv == 0 || ri == 0) return; // don't fold divide-by-zero
                expr = isFloat ? (std::unique_ptr<Expr>)std::make_unique<FloatLiteral>(lv / rv)
                               : std::make_unique<Number>(li / ri);
                break;
            default:
                break; // comparisons / logical ops – leave for runtime
        }
        return;
    }

    if (auto* unary = dynamic_cast<UnaryExpr*>(expr.get())) {
        foldExpr(unary->operand);
        if (unary->op == TokenType::MINUS) {
            if (auto* n = dynamic_cast<Number*>(unary->operand.get()))
                expr = std::make_unique<Number>(-n->value);
            else if (auto* f = dynamic_cast<FloatLiteral*>(unary->operand.get()))
                expr = std::make_unique<FloatLiteral>(-f->value);
        }
        return;
    }

    if (auto* cast = dynamic_cast<CastExpr*>(expr.get())) {
        foldExpr(cast->operand);
        // Constant-fold cast of numeric literals
        if (cast->targetType == "int") {
            if (auto* f = dynamic_cast<FloatLiteral*>(cast->operand.get()))
                expr = std::make_unique<Number>((int)f->value);
        } else if (cast->targetType == "float") {
            if (auto* n = dynamic_cast<Number*>(cast->operand.get()))
                expr = std::make_unique<FloatLiteral>((double)n->value);
        }
        return;
    }

    if (auto* call = dynamic_cast<CallExpr*>(expr.get())) {
        for (auto& a : call->arguments) foldExpr(a);
        return;
    }
    if (auto* arr = dynamic_cast<ArrayLiteral*>(expr.get())) {
        for (auto& e : arr->elements) foldExpr(e);
        return;
    }
    if (auto* acc = dynamic_cast<ArrayAccess*>(expr.get())) {
        foldExpr(acc->index);
        return;
    }
    if (auto* om = dynamic_cast<ObjectMethodCall*>(expr.get())) {
        foldExpr(om->object);
        for (auto& a : om->arguments) foldExpr(a);
        return;
    }
    if (auto* oa = dynamic_cast<ObjectMemberAccess*>(expr.get())) {
        foldExpr(oa->object);
        return;
    }
}

static void foldAllStatements(std::vector<std::unique_ptr<Statement>>& stmts) {
    for (auto& s : stmts) foldStmt(s.get());
}

// ===========================================================================
// Helper: scan a body for any `return <value>;` at any nesting level
// ===========================================================================

static bool scanForValueReturn(const std::vector<std::unique_ptr<Statement>>& body) {
    for (const auto& s : body) {
        if (auto* ret = dynamic_cast<const Return*>(s.get()))
            if (ret->value) return true;
        if (auto* ifs = dynamic_cast<const IfStatement*>(s.get()))
            if (scanForValueReturn(ifs->thenBranch) || scanForValueReturn(ifs->elseBranch))
                return true;
        if (auto* ws = dynamic_cast<const WhileStatement*>(s.get()))
            if (scanForValueReturn(ws->body)) return true;
        if (auto* fs = dynamic_cast<const ForStatement*>(s.get()))
            if (scanForValueReturn(fs->body)) return true;
    }
    return false;
}

// ===========================================================================
// SemanticAnalyzer – helpers
// ===========================================================================

void SemanticAnalyzer::error(const std::string& msg, int line) {
    std::string full = msg;
    if (line > 0) full += " (line " + std::to_string(line) + ")";
    errors_.push_back({full, line});
}

void SemanticAnalyzer::warn(const std::string& msg, int line) {
    std::string full = msg;
    if (line > 0) full += " (line " + std::to_string(line) + ")";
    warnings_.push_back({full, line});
}

const ClassInfo* SemanticAnalyzer::resolveClass(const std::string& name) const {
    auto it = classes_.find(name);
    return it == classes_.end() ? nullptr : &it->second;
}

bool SemanticAnalyzer::typesCompatible(const std::string& declared,
                                        const std::string& actual) const {
    if (actual == "unknown") return true;
    if (declared == actual)  return true;
    // Implicit numeric widening: bool → int → float; char → int
    if (declared == "float"  && (actual == "int"  || actual == "bool" || actual == "char")) return true;
    if (declared == "int"    && (actual == "bool" || actual == "char")) return true;
    if (declared == "bool"   && actual == "int")  return true;
    return false;
}

bool SemanticAnalyzer::castAllowed(const std::string& to, const std::string& from) const {
    if (from == "unknown") return true;
    static const std::unordered_set<std::string> numeric = {"int","float","char","bool"};
    if (numeric.count(to) && numeric.count(from)) return true;
    if (to == "string") return true; // any → string
    return false;
}

std::string SemanticAnalyzer::findFieldType(const std::string& className,
                                             const std::string& field) const {
    std::string cur = className;
    while (!cur.empty()) {
        const auto* cls = resolveClass(cur);
        if (!cls) break;
        auto it = cls->fieldTypes.find(field);
        if (it != cls->fieldTypes.end()) return it->second;
        cur = cls->baseClass;
    }
    return "";
}

const FunctionInfo* SemanticAnalyzer::findMethod(const std::string& className,
                                                   const std::string& method) const {
    std::string cur = className;
    while (!cur.empty()) {
        const auto* cls = resolveClass(cur);
        if (!cls) break;
        auto it = cls->methods.find(method);
        if (it != cls->methods.end()) return &it->second;
        cur = cls->baseClass;
    }
    return nullptr;
}

bool SemanticAnalyzer::guaranteedReturn(
    const std::vector<std::unique_ptr<Statement>>& stmts) const
{
    for (const auto& s : stmts) {
        if (dynamic_cast<const Return*>(s.get())) return true;
        if (auto* ifs = dynamic_cast<const IfStatement*>(s.get())) {
            if (!ifs->elseBranch.empty() &&
                guaranteedReturn(ifs->thenBranch) &&
                guaranteedReturn(ifs->elseBranch))
                return true;
        }
    }
    return false;
}

// ===========================================================================
// Pass 1 – collect declarations
// ===========================================================================

void SemanticAnalyzer::firstPass(const std::vector<std::unique_ptr<Statement>>& stmts) {
    for (const auto& s : stmts) {
        if (auto* func = dynamic_cast<const FunctionDef*>(s.get())) {
            FunctionInfo info;
            info.returnType      = "unknown";
            info.params          = func->params;
            info.hasValueReturn  = scanForValueReturn(func->body);
            functions_[func->name] = std::move(info);

        } else if (auto* cls = dynamic_cast<const ClassDef*>(s.get())) {
            ClassInfo info;
            info.baseClass = cls->baseClass;
            for (const auto& f : cls->fields)
                info.fieldTypes[f.second] = f.first;
            for (const auto& m : cls->methods) {
                FunctionInfo minfo;
                minfo.params         = m->params;
                minfo.hasValueReturn = scanForValueReturn(m->body);
                info.methods[m->name] = std::move(minfo);
            }
            classes_[cls->name] = std::move(info);
        }
    }

    // Validate inheritance
    for (const auto& [name, info] : classes_)
        if (!info.baseClass.empty() && !classes_.count(info.baseClass))
            error("Class '" + name + "' inherits from undefined class '" + info.baseClass + "'");
}

// ===========================================================================
// Main entry point
// ===========================================================================

std::vector<SemanticError> SemanticAnalyzer::analyze(
    std::vector<std::unique_ptr<Statement>>& stmts)
{
    errors_.clear();
    warnings_.clear();
    symbols_         = SymbolTable{};
    functions_.clear();
    classes_.clear();
    currentClass_    = "";
    currentFunction_ = "";

    // Constant-folding pass (runs before type checking)
    foldAllStatements(stmts);

    firstPass(stmts);
    analyzeStatementList(stmts);
    return errors_;
}

// ===========================================================================
// Statement list – with unreachable-code detection
// ===========================================================================

bool SemanticAnalyzer::analyzeStatementList(
    const std::vector<std::unique_ptr<Statement>>& stmts)
{
    bool pastReturn = false;
    for (const auto& s : stmts) {
        if (pastReturn) {
            warn("Unreachable code after 'return'", s->line);
            break; // one warning per block is enough
        }
        analyzeStatement(s.get());

        // Did this statement guarantee exit from the block?
        if (dynamic_cast<const Return*>(s.get())) {
            pastReturn = true;
        } else if (auto* ifs = dynamic_cast<const IfStatement*>(s.get())) {
            if (!ifs->elseBranch.empty() &&
                guaranteedReturn(ifs->thenBranch) &&
                guaranteedReturn(ifs->elseBranch))
                pastReturn = true;
        }
    }
    return pastReturn;
}

// ===========================================================================
// Statement analysis
// ===========================================================================

void SemanticAnalyzer::analyzeStatement(const Statement* stmt) {
    if (!stmt) return;

    // ── Variable declaration / assignment ─────────────────────────────────
    if (auto* assign = dynamic_cast<const Assignment*>(stmt)) {
        const std::string& nm = assign->name;

        std::string inferredType = "unknown";
        if (assign->value)
            inferredType = analyzeExpr(assign->value.get());

        if (!assign->type.empty()) {
            // Typed declaration → new variable
            std::string declType = assign->type;
            bool isArray = declType.size() > 2 &&
                           declType.substr(declType.size() - 2) == "[]";
            std::string baseType = isArray ? declType.substr(0, declType.size() - 2) : declType;

            static const std::unordered_set<std::string> primitives =
                {"int","float","char","bool","string"};
            if (!primitives.count(baseType) && !classes_.count(baseType))
                error("Unknown type '" + baseType + "' in declaration of '" + nm + "'", stmt->line);

            if (nm.find('.') == std::string::npos) {
                // Shadow warning: same name exists in an outer scope
                if (symbols_.existsInOuter(nm))
                    warn("Variable '" + nm + "' shadows an outer declaration", stmt->line);

                if (symbols_.existsInCurrentScope(nm))
                    error("Variable '" + nm + "' already declared in this scope", stmt->line);

                // Type mismatch check (skip for arrays – size/literal doesn't match element type)
                if (assign->value && !isArray &&
                    inferredType != "unknown" && !typesCompatible(baseType, inferredType)) {
                    error("Type mismatch: cannot assign '" + inferredType +
                          "' to '" + baseType + "' variable '" + nm + "'", stmt->line);
                }

                bool initialized = assign->value != nullptr;
                symbols_.declare(nm, declType, initialized);
            }
        } else {
            // Re-assignment (no type keyword)
            if (nm.find('.') == std::string::npos && nm.find('[') == std::string::npos) {
                const auto* sym = symbols_.lookup(nm);
                if (!sym) {
                    bool isField = !currentClass_.empty() &&
                                   !findFieldType(currentClass_, nm).empty();
                    if (!isField)
                        error("Assignment to undeclared variable '" + nm + "'", stmt->line);
                } else {
                    symbols_.setInitialized(nm);
                }
            }
        }
        return;
    }

    // ── Print ─────────────────────────────────────────────────────────────
    if (auto* print = dynamic_cast<const Print*>(stmt)) {
        analyzeExpr(print->value.get());
        return;
    }

    // ── Function definition ───────────────────────────────────────────────
    if (auto* func = dynamic_cast<const FunctionDef*>(stmt)) {
        std::string prevFunc = currentFunction_;
        currentFunction_     = func->name;
        symbols_.pushScope();
        for (const auto& [type, name] : func->params)
            symbols_.declare(name, type.empty() ? "unknown" : type);

        bool returned = analyzeStatementList(func->body);

        // Return checking: if the function has any value-returning path,
        // all paths must return.
        auto it = functions_.find(func->name);
        if (it != functions_.end() && it->second.hasValueReturn && !returned)
            error("Function '" + func->name +
                  "' may not return a value on all code paths", stmt->line);
        if (!it->second.hasValueReturn && func->body.empty())
            warn("Function '" + func->name + "' has an empty body", stmt->line);

        symbols_.popScope();
        currentFunction_ = prevFunc;
        return;
    }

    // ── Return ────────────────────────────────────────────────────────────
    if (auto* ret = dynamic_cast<const Return*>(stmt)) {
        if (currentFunction_.empty() && currentClass_.empty())
            error("'return' used outside of a function", stmt->line);
        if (ret->value) analyzeExpr(ret->value.get());
        return;
    }

    // ── If ────────────────────────────────────────────────────────────────
    if (auto* ifs = dynamic_cast<const IfStatement*>(stmt)) {
        analyzeExpr(ifs->condition.get());
        symbols_.pushScope();
        analyzeStatementList(ifs->thenBranch);
        symbols_.popScope();
        symbols_.pushScope();
        analyzeStatementList(ifs->elseBranch);
        symbols_.popScope();
        return;
    }

    // ── While ─────────────────────────────────────────────────────────────
    if (auto* ws = dynamic_cast<const WhileStatement*>(stmt)) {
        analyzeExpr(ws->condition.get());
        symbols_.pushScope();
        analyzeStatementList(ws->body);
        symbols_.popScope();
        return;
    }

    // ── For ───────────────────────────────────────────────────────────────
    if (auto* fs = dynamic_cast<const ForStatement*>(stmt)) {
        symbols_.pushScope();
        if (fs->initializer) analyzeStatement(fs->initializer.get());
        if (fs->condition)   analyzeExpr(fs->condition.get());
        if (fs->increment)   analyzeStatement(fs->increment.get());
        analyzeStatementList(fs->body);
        symbols_.popScope();
        return;
    }

    // ── Expression statement ──────────────────────────────────────────────
    if (auto* es = dynamic_cast<const ExprStatement*>(stmt)) {
        analyzeExpr(es->expr.get());
        return;
    }

    // ── Array element assignment ──────────────────────────────────────────
    if (auto* aa = dynamic_cast<const ArrayAssignment*>(stmt)) {
        analyzeExpr(aa->index.get());
        analyzeExpr(aa->value.get());
        if (!symbols_.lookup(aa->arrayName))
            error("Assignment to undeclared array '" + aa->arrayName + "'", stmt->line);
        return;
    }

    // ── Class definition ──────────────────────────────────────────────────
    if (auto* cls = dynamic_cast<const ClassDef*>(stmt)) {
        std::string prevClass = currentClass_;
        currentClass_         = cls->name;

        for (const auto& method : cls->methods) {
            std::string prevFunc = currentFunction_;
            currentFunction_     = method->name;
            symbols_.pushScope();

            // Bring all fields (including inherited) into method scope
            std::string c = cls->name;
            while (!c.empty()) {
                const auto* ci = resolveClass(c);
                if (!ci) break;
                for (const auto& [fname, ftype] : ci->fieldTypes)
                    symbols_.declare(fname, ftype);
                c = ci->baseClass;
            }
            for (const auto& [type, name] : method->params)
                symbols_.declare(name, type.empty() ? "unknown" : type);

            bool returned = analyzeStatementList(method->body);

            // Check method return completeness
            const auto* clsInfo = resolveClass(cls->name);
            if (clsInfo) {
                auto mit = clsInfo->methods.find(method->name);
                if (mit != clsInfo->methods.end() &&
                    mit->second.hasValueReturn && !returned)
                    error("Method '" + cls->name + "::" + method->name +
                          "' may not return a value on all code paths", stmt->line);
            }

            symbols_.popScope();
            currentFunction_ = prevFunc;
        }
        currentClass_ = prevClass;
        return;
    }

    // ── Object instantiation ──────────────────────────────────────────────
    if (auto* oi = dynamic_cast<const ObjectInstantiation*>(stmt)) {
        if (!classes_.count(oi->className)) {
            error("Instantiation of undefined class '" + oi->className + "'", stmt->line);
        } else {
            const auto* mi = findMethod(oi->className, "init");
            if (mi && oi->arguments.size() != mi->params.size())
                error("Constructor 'init' of '" + oi->className + "' expects " +
                      std::to_string(mi->params.size()) + " argument(s) but got " +
                      std::to_string(oi->arguments.size()), stmt->line);
        }
        for (const auto& arg : oi->arguments) analyzeExpr(arg.get());
        symbols_.declare(oi->varName, oi->className);
        return;
    }

    // ── Import (already resolved) ─────────────────────────────────────────
    if (dynamic_cast<const ImportStatement*>(stmt)) return;
}

// ===========================================================================
// Expression analysis
// ===========================================================================

std::string SemanticAnalyzer::analyzeExpr(const Expr* expr) {
    if (!expr) return "unknown";

    if (dynamic_cast<const Number*>(expr))        return "int";
    if (dynamic_cast<const FloatLiteral*>(expr))  return "float";
    if (dynamic_cast<const CharLiteral*>(expr))   return "char";
    if (dynamic_cast<const BoolLiteral*>(expr))   return "bool";
    if (dynamic_cast<const StringLiteral*>(expr)) return "string";
    if (dynamic_cast<const InputExpr*>(expr))     return "int";
    if (dynamic_cast<const ReadExpr*>(expr))      return "int";

    if (auto* arr = dynamic_cast<const ArrayLiteral*>(expr)) {
        for (const auto& e : arr->elements) analyzeExpr(e.get());
        return "array";
    }

    // ── Explicit cast ─────────────────────────────────────────────────────
    if (auto* cast = dynamic_cast<const CastExpr*>(expr)) {
        std::string fromType = analyzeExpr(cast->operand.get());
        if (!castAllowed(cast->targetType, fromType))
            error("Cannot cast '" + fromType + "' to '" + cast->targetType + "'", expr->line);
        return cast->targetType;
    }

    // ── Variable reference ────────────────────────────────────────────────
    if (auto* var = dynamic_cast<const Variable*>(expr)) {
        if (var->name == "this")  return currentClass_;
        if (var->name == "super") {
            const auto* cls = resolveClass(currentClass_);
            return cls ? cls->baseClass : "unknown";
        }

        const auto* sym = symbols_.lookup(var->name);
        if (sym) {
            if (!sym->initialized)
                warn("Variable '" + var->name + "' may be used before initialization", expr->line);
            return sym->type;
        }

        // Bare field reference inside a class method
        if (!currentClass_.empty()) {
            std::string ft = findFieldType(currentClass_, var->name);
            if (!ft.empty()) return ft;
        }

        error("Use of undeclared variable '" + var->name + "'", expr->line);
        return "unknown";
    }

    // ── Unary ─────────────────────────────────────────────────────────────
    if (auto* unary = dynamic_cast<const UnaryExpr*>(expr)) {
        std::string t = analyzeExpr(unary->operand.get());
        return (unary->op == TokenType::NOT) ? "bool" : t;
    }

    // ── Binary ────────────────────────────────────────────────────────────
    if (auto* bin = dynamic_cast<const BinaryExpr*>(expr)) {
        std::string lt = analyzeExpr(bin->left.get());
        std::string rt = analyzeExpr(bin->right.get());
        TokenType op = bin->op;

        if (op == TokenType::GREATERTHEN || op == TokenType::LESSTHEN  ||
            op == TokenType::EQUALTO     || op == TokenType::NOTEQUALTO ||
            op == TokenType::AND         || op == TokenType::OR)
            return "bool";

        if (op == TokenType::PLUS && (lt == "string" || rt == "string"))
            return "string";

        if (lt == "float" || rt == "float") return "float";
        return "int";
    }

    // ── Function call ─────────────────────────────────────────────────────
    if (auto* call = dynamic_cast<const CallExpr*>(expr)) {
        auto it = functions_.find(call->callee);
        if (it == functions_.end()) {
            error("Call to undefined function '" + call->callee + "'", expr->line);
            for (const auto& a : call->arguments) analyzeExpr(a.get());
            return "unknown";
        }
        if (call->arguments.size() != it->second.params.size())
            error("Function '" + call->callee + "' expects " +
                  std::to_string(it->second.params.size()) + " argument(s) but got " +
                  std::to_string(call->arguments.size()), expr->line);
        for (const auto& a : call->arguments) analyzeExpr(a.get());
        return it->second.returnType;
    }

    // ── Array access ──────────────────────────────────────────────────────
    if (auto* arrAcc = dynamic_cast<const ArrayAccess*>(expr)) {
        analyzeExpr(arrAcc->index.get());
        const auto* sym = symbols_.lookup(arrAcc->arrayName);
        if (!sym) {
            error("Use of undeclared array '" + arrAcc->arrayName + "'", expr->line);
            return "unknown";
        }
        std::string t = sym->type;
        if (t.size() > 2 && t.substr(t.size() - 2) == "[]")
            t = t.substr(0, t.size() - 2);
        return t;
    }

    // ── Member access (obj.field) ─────────────────────────────────────────
    if (auto* objAcc = dynamic_cast<const ObjectMemberAccess*>(expr)) {
        if (auto* v = dynamic_cast<const Variable*>(objAcc->object.get())) {
            if (v->name == "this" || v->name == "super") {
                std::string lookIn = (v->name == "super") ?
                    (resolveClass(currentClass_) ? resolveClass(currentClass_)->baseClass : "")
                    : currentClass_;
                std::string ft = findFieldType(lookIn, objAcc->member);
                if (ft.empty())
                    error("'" + lookIn + "' has no field '" + objAcc->member + "'", expr->line);
                return ft.empty() ? "unknown" : ft;
            }
        }
        std::string objType = analyzeExpr(objAcc->object.get());
        std::string ft = findFieldType(objType, objAcc->member);
        if (ft.empty() && resolveClass(objType))
            error("Class '" + objType + "' has no field '" + objAcc->member + "'", expr->line);
        return ft.empty() ? "unknown" : ft;
    }

    // ── Method call (obj.method(args)) ────────────────────────────────────
    if (auto* objMeth = dynamic_cast<const ObjectMethodCall*>(expr)) {
        for (const auto& a : objMeth->arguments) analyzeExpr(a.get());

        if (auto* v = dynamic_cast<const Variable*>(objMeth->object.get())) {
            if (v->name == "this" || v->name == "super") {
                std::string lookIn = (v->name == "super") ?
                    (resolveClass(currentClass_) ? resolveClass(currentClass_)->baseClass : "")
                    : currentClass_;
                const FunctionInfo* mi = findMethod(lookIn, objMeth->method);
                if (!mi)
                    error("Method '" + objMeth->method + "' not found in '" + lookIn + "'",
                          expr->line);
                else if (objMeth->arguments.size() != mi->params.size())
                    error("Method '" + objMeth->method + "' expects " +
                          std::to_string(mi->params.size()) + " argument(s) but got " +
                          std::to_string(objMeth->arguments.size()), expr->line);
                return mi ? mi->returnType : "unknown";
            }
        }

        std::string objType = analyzeExpr(objMeth->object.get());
        if (!resolveClass(objType)) return "unknown";

        const FunctionInfo* mi = findMethod(objType, objMeth->method);
        if (!mi) {
            error("Method '" + objMeth->method + "' not found in class '" + objType + "'",
                  expr->line);
            return "unknown";
        }
        if (objMeth->arguments.size() != mi->params.size())
            error("Method '" + objMeth->method + "' expects " +
                  std::to_string(mi->params.size()) + " argument(s) but got " +
                  std::to_string(objMeth->arguments.size()), expr->line);
        return mi->returnType;
    }

    return "unknown";
}

// ===========================================================================
// Public interface
// ===========================================================================

void semanticAnalyze(std::vector<std::unique_ptr<Statement>>& stmts) {
    SemanticAnalyzer analyzer;
    auto errors = analyzer.analyze(stmts);

    const auto& warnings = analyzer.warnings();

    if (!warnings.empty()) {
        std::cerr << "\nSemantic Warnings (" << warnings.size() << "):\n";
        for (size_t i = 0; i < warnings.size(); ++i)
            std::cerr << "  [W" << (i + 1) << "] " << warnings[i].message << "\n";
    }

    if (!errors.empty()) {
        std::cerr << "\nSemantic Errors (" << errors.size() << "):\n";
        for (size_t i = 0; i < errors.size(); ++i)
            std::cerr << "  [E" << (i + 1) << "] " << errors[i].message << "\n";
        std::cerr << "\n";
        throw std::runtime_error("Compilation aborted: semantic errors found.");
    }

    if (!warnings.empty()) std::cerr << "\n";
}
