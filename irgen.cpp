#include "irgen.hpp"
#include "lexer.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

// ===========================================================================
// Helpers
// ===========================================================================

std::string IRGen::newLabel(const std::string& pfx) {
    return "__" + pfx + std::to_string(labelCount_++);
}

void IRGen::emit(IROp op, const std::string& s, int i, double d, char c) {
    cur_->push_back({op, s, i, d, c});
}

// ===========================================================================
// Top-level entry point
// ===========================================================================

IRProgram IRGen::generate(const std::vector<std::unique_ptr<Statement>>& stmts) {
    prog_        = {};
    cur_         = &prog_.main;
    labelCount_  = 0;
    currentClass_.clear();
    currentParams_.clear();
    currentFields_.clear();

    // Pass 1 – collect class metadata and compile all functions/methods
    firstPass(stmts);

    // Pass 2 – generate main-level code (skip class/function/import defs)
    for (auto& st : stmts) {
        if (dynamic_cast<const ClassDef*>(st.get()))      continue;
        if (dynamic_cast<const FunctionDef*>(st.get()))   continue;
        if (dynamic_cast<const ImportStatement*>(st.get())) continue;
        genStmt(st.get());
    }
    return prog_;
}

// ===========================================================================
// First pass – class metadata + function compilation
// ===========================================================================

void IRGen::firstPass(const std::vector<std::unique_ptr<Statement>>& stmts) {
    // Register all classes first (needed by collectAllFields during method compile)
    for (auto& st : stmts) {
        if (auto cls = dynamic_cast<const ClassDef*>(st.get())) {
            IRClass irCls;
            irCls.name      = cls->name;
            irCls.baseClass = cls->baseClass;
            irCls.fields    = cls->fields;
            prog_.classes[cls->name] = std::move(irCls);
        }
    }
    // Now compile functions and methods
    for (auto& st : stmts) {
        if (auto cls = dynamic_cast<const ClassDef*>(st.get())) {
            for (auto& m : cls->methods)
                compileFunction(m.get(), cls->name);
        } else if (auto fn = dynamic_cast<const FunctionDef*>(st.get())) {
            compileFunction(fn);
        }
    }
}

void IRGen::collectAllFields(const std::string& className,
                              std::vector<std::pair<std::string,std::string>>& out) const {
    auto it = prog_.classes.find(className);
    if (it == prog_.classes.end()) return;
    if (!it->second.baseClass.empty())
        collectAllFields(it->second.baseClass, out);
    for (auto& f : it->second.fields) {
        auto fi = std::find_if(out.begin(), out.end(),
                               [&](const auto& x){ return x.second == f.second; });
        if (fi != out.end()) *fi = f; else out.push_back(f);
    }
}

void IRGen::compileFunction(const FunctionDef* func, const std::string& cls) {
    IRFunction irFn;
    irFn.name      = func->name;
    irFn.className = cls;
    irFn.params    = func->params;

    // Save outer context
    auto* savedCur    = cur_;
    auto  savedCls    = currentClass_;
    auto  savedParams = currentParams_;
    auto  savedFields = currentFields_;

    currentClass_ = cls;
    currentParams_.clear();
    for (auto& [t, n] : func->params) currentParams_.insert(n);
    currentFields_.clear();
    if (!cls.empty()) collectAllFields(cls, currentFields_);

    cur_ = &irFn.code;

    for (auto& st : func->body) genStmt(st.get());

    // Implicit void return at end of function body
    emit(IROp::RETURN);

    // Restore
    cur_           = savedCur;
    currentClass_  = savedCls;
    currentParams_ = savedParams;
    currentFields_ = savedFields;

    std::string key = cls.empty() ? func->name : cls + "::" + func->name;
    prog_.functions[key] = std::move(irFn);
}

// ===========================================================================
// Statement code generation
// ===========================================================================

void IRGen::genStmt(const Statement* stmt) {
    // Already compiled – skip
    if (dynamic_cast<const FunctionDef*>(stmt))    return;
    if (dynamic_cast<const ClassDef*>(stmt))        return;
    if (dynamic_cast<const ImportStatement*>(stmt)) return;

    // ------------------------------------------------------------------
    // Print
    // ------------------------------------------------------------------
    if (auto pr = dynamic_cast<const Print*>(stmt)) {
        genExpr(pr->value.get());
        emit(IROp::PRINT);
        return;
    }

    // ------------------------------------------------------------------
    // Return
    // ------------------------------------------------------------------
    if (auto ret = dynamic_cast<const Return*>(stmt)) {
        if (ret->value) { genExpr(ret->value.get()); emit(IROp::RETURN_VAL); }
        else              emit(IROp::RETURN);
        return;
    }

    // ------------------------------------------------------------------
    // ObjectInstantiation  (Person p("Alice", 30);)
    // ------------------------------------------------------------------
    if (auto oi = dynamic_cast<const ObjectInstantiation*>(stmt)) {
        for (auto& a : oi->arguments) genExpr(a.get());
        emit(IROp::NEW_OBJ, oi->className, (int)oi->arguments.size());
        emit(IROp::DECLARE, oi->varName);
        return;
    }

    // ------------------------------------------------------------------
    // ArrayAssignment  (arr[i] = val;)
    // ------------------------------------------------------------------
    if (auto aa = dynamic_cast<const ArrayAssignment*>(stmt)) {
        emit(IROp::LOAD, aa->arrayName);
        genExpr(aa->index.get());
        genExpr(aa->value.get());
        emit(IROp::ARRAY_STORE);
        return;
    }

    // ------------------------------------------------------------------
    // Assignment / variable declaration
    // ------------------------------------------------------------------
    if (auto asgn = dynamic_cast<const Assignment*>(stmt)) {

        // Object-array declaration: "ClassName[] arr = size_expr;"
        if (!asgn->type.empty() && asgn->type.size() > 2 &&
            asgn->type.substr(asgn->type.size()-2) == "[]") {
            std::string elemType = asgn->type.substr(0, asgn->type.size()-2);
            if (asgn->value) genExpr(asgn->value.get());
            else             emit(IROp::PUSH_INT, "", 0);
            emit(IROp::NEW_ARRAY, elemType, -1); // -1 = size on stack
            emit(IROp::DECLARE, asgn->name);
            return;
        }

        // Class-typed declaration without constructor: "Person p;" (no args)
        if (!asgn->type.empty() && prog_.classes.count(asgn->type) && !asgn->value) {
            emit(IROp::NEW_OBJ, asgn->type, 0);
            emit(IROp::DECLARE, asgn->name);
            return;
        }

        // Field assignment through dot: "obj.field = value" / "arr[i].field = value"
        if (asgn->name.find('.') != std::string::npos) {
            size_t dot     = asgn->name.find('.');
            std::string objPart = asgn->name.substr(0, dot);
            std::string field   = asgn->name.substr(dot + 1);

            // "this.field = value" inside a method → treat as bare field update (fields-as-locals)
            if (objPart == "this") {
                genExpr(asgn->value.get());
                emit(IROp::STORE, field);
                return;
            }

            // "arr[idx].field = value"
            size_t lb = objPart.find('[');
            if (lb != std::string::npos) {
                std::string arrName = objPart.substr(0, lb);
                std::string idxStr  = objPart.substr(lb + 1, objPart.size() - lb - 2);
                emit(IROp::LOAD, arrName);
                try { emit(IROp::PUSH_INT, "", std::stoi(idxStr)); }
                catch (...) { emit(IROp::LOAD, idxStr); }
                emit(IROp::ARRAY_LOAD); // push obj handle from array element
                genExpr(asgn->value.get());
                emit(IROp::STORE_FIELD, field);
            } else {
                // "obj.field = value"
                emit(IROp::LOAD, objPart);
                genExpr(asgn->value.get());
                emit(IROp::STORE_FIELD, field);
            }
            return;
        }

        // Array literal initialisation: "int[] arr = {1, 2, 3};"
        if (asgn->value) {
            if (auto al = dynamic_cast<const ArrayLiteral*>(asgn->value.get())) {
                for (auto& el : al->elements) genExpr(el.get());
                std::string elemType = "int";
                if (!asgn->type.empty() && asgn->type.back() == ']')
                    elemType = asgn->type.substr(0, asgn->type.size()-2);
                else if (!asgn->type.empty())
                    elemType = asgn->type;
                emit(IROp::NEW_ARRAY, elemType, (int)al->elements.size());
                // Declarations always use DECLARE; bare re-assignments use STORE
                if (!asgn->type.empty()) emit(IROp::DECLARE, asgn->name);
                else                     emit(IROp::STORE,   asgn->name);
                return;
            }
            genExpr(asgn->value.get());
        } else {
            // Typed declaration without initializer: "int x;" → default 0
            emit(IROp::PUSH_INT, "", 0);
        }

        if (!asgn->type.empty()) emit(IROp::DECLARE, asgn->name);
        else                     emit(IROp::STORE,   asgn->name);
        return;
    }

    // ------------------------------------------------------------------
    // IfStatement
    // ------------------------------------------------------------------
    if (auto ifs = dynamic_cast<const IfStatement*>(stmt)) {
        std::string elseL = newLabel("else");
        std::string endL  = newLabel("endif");

        genExpr(ifs->condition.get());
        emit(IROp::JUMP_FALSE, elseL);

        emit(IROp::ENTER_SCOPE);
        for (auto& s : ifs->thenBranch) genStmt(s.get());
        emit(IROp::EXIT_SCOPE);
        emit(IROp::JUMP, endL);

        emit(IROp::LABEL, elseL);
        if (!ifs->elseBranch.empty()) {
            emit(IROp::ENTER_SCOPE);
            for (auto& s : ifs->elseBranch) genStmt(s.get());
            emit(IROp::EXIT_SCOPE);
        }
        emit(IROp::LABEL, endL);
        return;
    }

    // ------------------------------------------------------------------
    // WhileStatement
    // ------------------------------------------------------------------
    if (auto ws = dynamic_cast<const WhileStatement*>(stmt)) {
        std::string startL = newLabel("while");
        std::string endL   = newLabel("endwhile");

        emit(IROp::LABEL, startL);
        genExpr(ws->condition.get());
        emit(IROp::JUMP_FALSE, endL);

        emit(IROp::ENTER_SCOPE);
        for (auto& s : ws->body) genStmt(s.get());
        emit(IROp::EXIT_SCOPE);

        emit(IROp::JUMP, startL);
        emit(IROp::LABEL, endL);
        return;
    }

    // ------------------------------------------------------------------
    // ForStatement
    // ------------------------------------------------------------------
    if (auto fs = dynamic_cast<const ForStatement*>(stmt)) {
        std::string startL = newLabel("for");
        std::string endL   = newLabel("endfor");

        emit(IROp::ENTER_SCOPE);                              // scope for loop var
        if (fs->initializer) genStmt(fs->initializer.get()); // e.g. DECLARE i = 0

        emit(IROp::LABEL, startL);
        if (fs->condition) {
            genExpr(fs->condition.get());
            emit(IROp::JUMP_FALSE, endL);
        }

        emit(IROp::ENTER_SCOPE);                              // scope for body
        for (auto& s : fs->body) genStmt(s.get());
        emit(IROp::EXIT_SCOPE);

        if (fs->increment) genStmt(fs->increment.get()); // e.g. STORE i
        emit(IROp::JUMP, startL);
        emit(IROp::LABEL, endL);
        emit(IROp::EXIT_SCOPE); // pop loop-var scope
        return;
    }

    // ------------------------------------------------------------------
    // ExprStatement  (function/method call used as statement)
    // ------------------------------------------------------------------
    if (auto es = dynamic_cast<const ExprStatement*>(stmt)) {
        genExpr(es->expr.get());
        emit(IROp::POP); // discard unused return value
        return;
    }

    throw std::runtime_error("IRGen: unsupported statement type");
}

// ===========================================================================
// Expression code generation  (always leaves exactly one value on the stack)
// ===========================================================================

void IRGen::genExpr(const Expr* expr) {

    if (auto n = dynamic_cast<const Number*>(expr)) {
        emit(IROp::PUSH_INT, "", n->value);
        return;
    }
    if (auto fl = dynamic_cast<const FloatLiteral*>(expr)) {
        emit(IROp::PUSH_FLOAT, "", 0, fl->value);
        return;
    }
    if (auto ch = dynamic_cast<const CharLiteral*>(expr)) {
        emit(IROp::PUSH_CHAR, "", 0, 0.0, ch->value);
        return;
    }
    if (auto bl = dynamic_cast<const BoolLiteral*>(expr)) {
        emit(IROp::PUSH_BOOL, "", bl->value ? 1 : 0);
        return;
    }
    if (auto sl = dynamic_cast<const StringLiteral*>(expr)) {
        emit(IROp::PUSH_STR, sl->value);
        return;
    }
    if (dynamic_cast<const InputExpr*>(expr)) {
        emit(IROp::INPUT);
        return;
    }
    if (auto re = dynamic_cast<const ReadExpr*>(expr)) {
        emit(IROp::READ_FILE, re->filename);
        return;
    }
    if (auto v = dynamic_cast<const Variable*>(expr)) {
        if (v->name == "this") { emit(IROp::PUSH_THIS); return; }
        emit(IROp::LOAD, v->name);
        return;
    }
    if (auto un = dynamic_cast<const UnaryExpr*>(expr)) {
        genExpr(un->operand.get());
        if      (un->op == TokenType::MINUS) emit(IROp::NEG);
        else if (un->op == TokenType::NOT)   emit(IROp::NOT);
        return;
    }
    if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
        genExpr(bin->left.get());
        genExpr(bin->right.get());
        switch (bin->op) {
            case TokenType::PLUS:           emit(IROp::ADD);     break;
            case TokenType::MINUS:          emit(IROp::SUB);     break;
            case TokenType::MULTIPLICATION: emit(IROp::MUL);     break;
            case TokenType::DIVISION:       emit(IROp::DIV);     break;
            case TokenType::EQUALTO:        emit(IROp::CMP_EQ);  break;
            case TokenType::NOTEQUALTO:     emit(IROp::CMP_NEQ); break;
            case TokenType::LESSTHEN:       emit(IROp::CMP_LT);  break;
            case TokenType::GREATERTHEN:    emit(IROp::CMP_GT);  break;
            case TokenType::AND:            emit(IROp::AND);      break;
            case TokenType::OR:             emit(IROp::OR);       break;
            default: throw std::runtime_error("IRGen: unsupported binary operator");
        }
        return;
    }
    if (auto ce = dynamic_cast<const CastExpr*>(expr)) {
        genExpr(ce->operand.get());
        if      (ce->targetType == "int")   emit(IROp::CAST_INT);
        else if (ce->targetType == "float") emit(IROp::CAST_FLOAT);
        else if (ce->targetType == "char")  emit(IROp::CAST_CHAR);
        else if (ce->targetType == "bool")  emit(IROp::CAST_BOOL);
        else                                emit(IROp::CAST_STR);
        return;
    }
    if (auto call = dynamic_cast<const CallExpr*>(expr)) {
        for (auto& a : call->arguments) genExpr(a.get());
        emit(IROp::CALL, call->callee, (int)call->arguments.size());
        return;
    }
    if (auto aa = dynamic_cast<const ArrayAccess*>(expr)) {
        emit(IROp::LOAD, aa->arrayName);
        genExpr(aa->index.get());
        emit(IROp::ARRAY_LOAD);
        return;
    }
    if (auto al = dynamic_cast<const ArrayLiteral*>(expr)) {
        for (auto& el : al->elements) genExpr(el.get());
        emit(IROp::NEW_ARRAY, "int", (int)al->elements.size());
        return;
    }
    if (auto oma = dynamic_cast<const ObjectMemberAccess*>(expr)) {
        if (auto v = dynamic_cast<const Variable*>(oma->object.get())) {
            if (v->name == "this") {
                emit(IROp::PUSH_THIS);
                emit(IROp::LOAD_FIELD, oma->member);
                return;
            }
        }
        genExpr(oma->object.get()); // push obj handle (or array handle for arr[i].field)
        emit(IROp::LOAD_FIELD, oma->member);
        return;
    }
    if (auto omc = dynamic_cast<const ObjectMethodCall*>(expr)) {
        if (auto v = dynamic_cast<const Variable*>(omc->object.get())) {
            if (v->name == "super") {
                // super.method(args) – use CALL_SUPER; no obj handle pushed
                for (auto& a : omc->arguments) genExpr(a.get());
                emit(IROp::CALL_SUPER, omc->method, (int)omc->arguments.size());
                return;
            }
        }
        // Regular obj.method(args) – push obj handle, then args, then CALL_METHOD
        genExpr(omc->object.get());
        for (auto& a : omc->arguments) genExpr(a.get());
        emit(IROp::CALL_METHOD, omc->method, (int)omc->arguments.size());
        return;
    }

    throw std::runtime_error("IRGen: unsupported expression type");
}

// ===========================================================================
// Human-readable IR dump
// ===========================================================================

static const char* opName(IROp op) {
    switch (op) {
        case IROp::PUSH_INT:    return "PUSH_INT";
        case IROp::PUSH_FLOAT:  return "PUSH_FLOAT";
        case IROp::PUSH_STR:    return "PUSH_STR";
        case IROp::PUSH_CHAR:   return "PUSH_CHAR";
        case IROp::PUSH_BOOL:   return "PUSH_BOOL";
        case IROp::LOAD:        return "LOAD";
        case IROp::DECLARE:     return "DECLARE";
        case IROp::STORE:       return "STORE";
        case IROp::POP:         return "POP";
        case IROp::ADD:         return "ADD";
        case IROp::SUB:         return "SUB";
        case IROp::MUL:         return "MUL";
        case IROp::DIV:         return "DIV";
        case IROp::NEG:         return "NEG";
        case IROp::CMP_EQ:      return "CMP_EQ";
        case IROp::CMP_NEQ:     return "CMP_NEQ";
        case IROp::CMP_LT:      return "CMP_LT";
        case IROp::CMP_GT:      return "CMP_GT";
        case IROp::AND:         return "AND";
        case IROp::OR:          return "OR";
        case IROp::NOT:         return "NOT";
        case IROp::JUMP:        return "JUMP";
        case IROp::JUMP_FALSE:  return "JUMP_FALSE";
        case IROp::LABEL:       return "LABEL";
        case IROp::CALL:        return "CALL";
        case IROp::RETURN:      return "RETURN";
        case IROp::RETURN_VAL:  return "RETURN_VAL";
        case IROp::PRINT:       return "PRINT";
        case IROp::INPUT:       return "INPUT";
        case IROp::READ_FILE:   return "READ_FILE";
        case IROp::CAST_INT:    return "CAST_INT";
        case IROp::CAST_FLOAT:  return "CAST_FLOAT";
        case IROp::CAST_CHAR:   return "CAST_CHAR";
        case IROp::CAST_BOOL:   return "CAST_BOOL";
        case IROp::CAST_STR:    return "CAST_STR";
        case IROp::PUSH_THIS:   return "PUSH_THIS";
        case IROp::NEW_OBJ:     return "NEW_OBJ";
        case IROp::LOAD_FIELD:  return "LOAD_FIELD";
        case IROp::STORE_FIELD: return "STORE_FIELD";
        case IROp::CALL_METHOD: return "CALL_METHOD";
        case IROp::CALL_SUPER:  return "CALL_SUPER";
        case IROp::NEW_ARRAY:   return "NEW_ARRAY";
        case IROp::ARRAY_LOAD:  return "ARRAY_LOAD";
        case IROp::ARRAY_STORE: return "ARRAY_STORE";
        case IROp::ENTER_SCOPE: return "ENTER_SCOPE";
        case IROp::EXIT_SCOPE:  return "EXIT_SCOPE";
        case IROp::NOP:         return "NOP";
    }
    return "?";
}

static void dumpCode(const std::vector<IRInstr>& code, const std::string& indent = "  ") {
    for (auto& ins : code) {
        if (ins.op == IROp::LABEL) { std::cout << ins.sval << ":\n"; continue; }
        std::cout << indent << opName(ins.op);
        if (!ins.sval.empty())  std::cout << " " << ins.sval;
        switch (ins.op) {
            case IROp::PUSH_INT:
            case IROp::PUSH_BOOL:
                std::cout << " " << ins.ival; break;
            case IROp::PUSH_FLOAT:
                std::cout << " " << ins.dval; break;
            case IROp::PUSH_CHAR:
                std::cout << " '" << ins.cval << "'"; break;
            case IROp::CALL:
            case IROp::CALL_METHOD:
            case IROp::CALL_SUPER:
            case IROp::NEW_OBJ:
            case IROp::NEW_ARRAY:
                std::cout << " " << ins.ival; break;
            default: break;
        }
        std::cout << "\n";
    }
}

void IRGen::dump(const IRProgram& prog) {
    std::cout << "=== TinyLang IR ===\n";
    for (auto& [key, fn] : prog.functions) {
        std::cout << "\n[func " << key << "](";
        for (size_t i = 0; i < fn.params.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << fn.params[i].first << " " << fn.params[i].second;
        }
        std::cout << "):\n";
        dumpCode(fn.code);
    }
    std::cout << "\n[main]:\n";
    dumpCode(prog.main);
    std::cout << "===================\n";
}

// ---------------------------------------------------------------------------
// Public wrappers
// ---------------------------------------------------------------------------
IRProgram generateIR(const std::vector<std::unique_ptr<Statement>>& stmts) {
    IRGen gen;
    return gen.generate(stmts);
}

void dumpIR(const IRProgram& prog) {
    IRGen::dump(prog);
}
