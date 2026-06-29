#include "tirgen.hpp"
#include "lexer.hpp"
#include <iostream>
#include <stdexcept>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Label / register helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string TIRGen::newLabel(const std::string& pfx) {
    return pfx + std::to_string(labelCount_++);
}

TIR::Reg TIRGen::freshReg() {
    return curFunc_->freshReg();
}

TIR::Block& TIRGen::curBlock() {
    return curFunc_->blocks[curBlockIdx_];
}

bool TIRGen::isSealed() const {
    if (curBlockIdx_ < 0) return true;
    return curFunc_->blocks[curBlockIdx_].sealed;
}

std::string TIRGen::addBlock(const std::string& label) {
    curFunc_->blocks.push_back({label, {}, {}, false});
    return label;
}

void TIRGen::switchBlock(const std::string& label) {
    for (int i = 0; i < (int)curFunc_->blocks.size(); ++i) {
        if (curFunc_->blocks[i].label == label) {
            curBlockIdx_ = i;
            return;
        }
    }
    throw std::runtime_error("TIRGen: block not found: " + label);
}

// ─────────────────────────────────────────────────────────────────────────────
// Emit helpers
// ─────────────────────────────────────────────────────────────────────────────

TIR::Reg TIRGen::emit(TIR::Op op, TIR::Type ty,
                       std::vector<TIR::Val> args,
                       std::string name, std::string name2, int ival) {
    if (isSealed()) return freshReg();  // dead code after terminator
    TIR::Reg dest = freshReg();
    TIR::Instr ins;
    ins.dest  = dest;
    ins.op    = op;
    ins.type  = ty;
    ins.args  = std::move(args);
    ins.name  = std::move(name);
    ins.name2 = std::move(name2);
    ins.ival  = ival;
    curBlock().instrs.push_back(std::move(ins));
    return dest;
}

void TIRGen::emitVoid(TIR::Op op, TIR::Type ty,
                       std::vector<TIR::Val> args,
                       std::string name, std::string name2, int ival) {
    if (isSealed()) return;
    TIR::Instr ins;
    ins.dest  = TIR::NOREG;
    ins.op    = op;
    ins.type  = ty;
    ins.args  = std::move(args);
    ins.name  = std::move(name);
    ins.name2 = std::move(name2);
    ins.ival  = ival;
    curBlock().instrs.push_back(std::move(ins));
}

void TIRGen::emitTerm(TIR::Term t) {
    if (isSealed()) return;
    curBlock().term   = std::move(t);
    curBlock().sealed = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Scope / variable helpers
// ─────────────────────────────────────────────────────────────────────────────

void TIRGen::pushScope() { scopes_.push_back({}); }
void TIRGen::popScope()  { if (!scopes_.empty()) scopes_.pop_back(); }

TIRGen::SlotInfo* TIRGen::findSlot(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return &f->second;
    }
    return nullptr;
}

TIR::Reg TIRGen::declareVar(const std::string& name, TIR::Type ty, TIR::Val init) {
    TIR::Reg slot = emit(TIR::Op::Alloc, ty, {}, name);
    emitVoid(TIR::Op::Store, ty,
             {init, TIR::Val::ofReg(slot, ty)});
    if (!scopes_.empty())
        scopes_.back()[name] = {slot, ty};
    return slot;
}

TIR::Val TIRGen::loadVar(const std::string& name) {
    SlotInfo* si = findSlot(name);
    if (!si)
        throw std::runtime_error("TIRGen: undefined variable: " + name);
    TIR::Reg vr = emit(TIR::Op::Load, si->type,
                        {TIR::Val::ofReg(si->reg, si->type)});
    return TIR::Val::ofReg(vr, si->type);
}

void TIRGen::storeVar(const std::string& name, TIR::Val val) {
    SlotInfo* si = findSlot(name);
    if (!si)
        throw std::runtime_error("TIRGen: undefined variable: " + name);
    emitVoid(TIR::Op::Store, si->type,
             {val, TIR::Val::ofReg(si->reg, si->type)});
}

// ─────────────────────────────────────────────────────────────────────────────
// Type helpers
// ─────────────────────────────────────────────────────────────────────────────

TIR::Type TIRGen::tyFromStr(const std::string& s) const {
    if (s == "int"  || s == "bool") return TIR::Type::i32();
    if (s == "float")               return TIR::Type::f64();
    if (s == "char")                return TIR::Type::char_();
    if (s == "string")              return TIR::Type::str();
    if (!s.empty() && prog_.classes.count(s))
        return TIR::Type::obj(s);
    // array type "T[]"
    if (s.size() > 2 && s.substr(s.size()-2) == "[]")
        return TIR::Type::arr(s.substr(0, s.size()-2));
    return TIR::Type::void_();
}

TIR::Type TIRGen::inferType(const Expr* e) const {
    if (dynamic_cast<const Number*>(e))       return TIR::Type::i32();
    if (dynamic_cast<const FloatLiteral*>(e)) return TIR::Type::f64();
    if (dynamic_cast<const BoolLiteral*>(e))  return TIR::Type::i1();
    if (dynamic_cast<const CharLiteral*>(e))  return TIR::Type::char_();
    if (dynamic_cast<const StringLiteral*>(e))return TIR::Type::str();
    if (auto v = dynamic_cast<const Variable*>(e)) {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto f = it->find(v->name);
            if (f != it->end()) return f->second.type;
        }
        return TIR::Type::void_();
    }
    if (auto bin = dynamic_cast<const BinaryExpr*>(e)) {
        switch (bin->op) {
        case TokenType::EQUALTO: case TokenType::NOTEQUALTO:
        case TokenType::LESSTHEN: case TokenType::GREATERTHEN:
        case TokenType::AND: case TokenType::OR:
            return TIR::Type::i1();
        default:
            return inferType(bin->left.get());
        }
    }
    if (auto cast = dynamic_cast<const CastExpr*>(e))
        return tyFromStr(cast->targetType);
    if (auto un = dynamic_cast<const UnaryExpr*>(e)) {
        if (un->op == TokenType::NOT) return TIR::Type::i1();
        return inferType(un->operand.get());
    }
    return TIR::Type::void_();
}

// ─────────────────────────────────────────────────────────────────────────────
// Class / field helpers
// ─────────────────────────────────────────────────────────────────────────────

void TIRGen::collectAllFields(const std::string& cls,
                               std::vector<std::pair<TIR::Type,std::string>>& out) const {
    auto it = prog_.classes.find(cls);
    if (it == prog_.classes.end()) return;
    if (!it->second.baseClass.empty())
        collectAllFields(it->second.baseClass, out);
    for (auto& field : it->second.fields) {
        const TIR::Type& fty   = field.first;
        const std::string& fname = field.second;
        auto fi = std::find_if(out.begin(), out.end(),
                               [&fname](const auto& x){ return x.second == fname; });
        if (fi != out.end()) fi->first = fty;
        else                 out.emplace_back(fty, fname);
    }
}

void TIRGen::emitFieldSync() {
    if (curThisReg_ == TIR::NOREG) return;
    TIR::Val thisVal = TIR::Val::ofReg(curThisReg_, TIR::Type::obj(curClass_));
    for (auto& field : curAllFields_) {
        const std::string& fName = field.second;
        SlotInfo* si = findSlot(fName);
        if (!si) continue;
        TIR::Reg vr = emit(TIR::Op::Load, si->type,
                           {TIR::Val::ofReg(si->reg, si->type)});
        emitVoid(TIR::Op::StoreField, si->type,
                 {TIR::Val::ofReg(vr, si->type), thisVal}, fName);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// First pass: register class metadata, compile all functions/methods
// ─────────────────────────────────────────────────────────────────────────────

void TIRGen::firstPass(const std::vector<std::unique_ptr<Statement>>& stmts) {
    // Register all classes first (needed by collectAllFields during method compile)
    for (auto& st : stmts) {
        if (auto cls = dynamic_cast<const ClassDef*>(st.get())) {
            TIR::Class tirCls;
            tirCls.name      = cls->name;
            tirCls.baseClass = cls->baseClass;
            for (auto& [fty, fn] : cls->fields)
                tirCls.fields.push_back({tyFromStr(fty), fn});
            prog_.classes[cls->name] = std::move(tirCls);
        }
    }
    // Compile all functions and class methods
    for (auto& st : stmts) {
        if (auto cls = dynamic_cast<const ClassDef*>(st.get())) {
            for (auto& m : cls->methods)
                compileFunction(m.get(), cls->name);
        } else if (auto fn = dynamic_cast<const FunctionDef*>(st.get())) {
            compileFunction(fn);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Compile a single function or method into a TIR::Func
// ─────────────────────────────────────────────────────────────────────────────

void TIRGen::compileFunction(const FunctionDef* fn, const std::string& cls) {
    TIR::Func tirFn;
    tirFn.name      = fn->name;
    tirFn.className = cls;
    for (auto& [tyStr, pname] : fn->params)
        tirFn.params.push_back({tyFromStr(tyStr), pname});

    // Save outer context
    auto* savedFunc       = curFunc_;
    int   savedBlockIdx   = curBlockIdx_;
    auto  savedClass      = curClass_;
    auto  savedParams     = curParams_;
    auto  savedAllFields  = curAllFields_;
    auto  savedThisReg    = curThisReg_;
    auto  savedScopes     = scopes_;

    // Set up new context
    curFunc_     = &tirFn;
    curBlockIdx_ = -1;
    curClass_    = cls;
    curParams_.clear();
    for (auto& [t, n] : fn->params) curParams_.insert(n);
    curAllFields_.clear();
    curThisReg_  = TIR::NOREG;
    scopes_.clear();

    addBlock("entry");
    switchBlock("entry");
    pushScope();

    // ── Field prologue (methods only) ──────────────────────────────────────
    if (!cls.empty()) {
        collectAllFields(cls, curAllFields_);
        // Emit: %this = this
        TIR::Reg tr  = emit(TIR::Op::PushThis, TIR::Type::obj(cls));
        curThisReg_  = tr;
        TIR::Val thisVal = TIR::Val::ofReg(tr, TIR::Type::obj(cls));

        for (auto& field : curAllFields_) {
            TIR::Type ft          = field.first;
            const std::string& fName = field.second;
            // alloc slot
            TIR::Reg slot = emit(TIR::Op::Alloc, ft, {}, fName);
            // load field value from object
            TIR::Reg fvr  = emit(TIR::Op::LoadField, ft, {thisVal}, fName);
            // initialise slot
            emitVoid(TIR::Op::Store, ft,
                     {TIR::Val::ofReg(fvr, ft), TIR::Val::ofReg(slot, ft)});
            scopes_.back()[fName] = {slot, ft};
        }
    }

    // ── Parameter slots ────────────────────────────────────────────────────
    for (int i = 0; i < (int)fn->params.size(); ++i) {
        auto& [tyStr, pname] = fn->params[i];
        TIR::Type ty = tyFromStr(tyStr);
        // %param_val = param i
        TIR::Reg pvr = emit(TIR::Op::ParamRef, ty, {}, "", "", i);
        declareVar(pname, ty, TIR::Val::ofReg(pvr, ty));
    }

    // ── Function body ──────────────────────────────────────────────────────
    for (auto& st : fn->body) genStmt(st.get());

    // ── Implicit void return ───────────────────────────────────────────────
    if (!isSealed()) {
        if (!cls.empty()) emitFieldSync();
        emitTerm(TIR::Term::ret());
    }

    popScope();

    // Register the function
    std::string key = cls.empty() ? fn->name : cls + "::" + fn->name;
    prog_.funcs[key] = std::move(tirFn);

    // Restore outer context
    curFunc_      = savedFunc;
    curBlockIdx_  = savedBlockIdx;
    curClass_     = savedClass;
    curParams_    = savedParams;
    curAllFields_ = savedAllFields;
    curThisReg_   = savedThisReg;
    scopes_       = savedScopes;
}

// ─────────────────────────────────────────────────────────────────────────────
// Statement code generation
// ─────────────────────────────────────────────────────────────────────────────

void TIRGen::genStmt(const Statement* stmt) {
    // Already compiled defs are skipped in pass 2
    if (dynamic_cast<const FunctionDef*>(stmt))    return;
    if (dynamic_cast<const ClassDef*>(stmt))       return;
    if (dynamic_cast<const ImportStatement*>(stmt)) return;

    // ── Print ──────────────────────────────────────────────────────────────
    if (auto pr = dynamic_cast<const Print*>(stmt)) {
        TIR::Val v = genExpr(pr->value.get());
        emitVoid(TIR::Op::Print, v.getType(), {v});
        return;
    }

    // ── Return ─────────────────────────────────────────────────────────────
    if (auto ret = dynamic_cast<const Return*>(stmt)) {
        if (ret->value) {
            TIR::Val v = genExpr(ret->value.get());
            if (!curClass_.empty()) emitFieldSync();
            emitTerm(TIR::Term::retVal(v));
        } else {
            if (!curClass_.empty()) emitFieldSync();
            emitTerm(TIR::Term::ret());
        }
        // Subsequent code is unreachable; open a dead block so emit doesn't crash.
        auto dead = newLabel("dead");
        addBlock(dead);
        switchBlock(dead);
        return;
    }

    // ── ObjectInstantiation  (Counter c(0);) ───────────────────────────────
    if (auto oi = dynamic_cast<const ObjectInstantiation*>(stmt)) {
        std::vector<TIR::Val> args;
        for (auto& a : oi->arguments) args.push_back(genExpr(a.get()));
        TIR::Reg obj = emit(TIR::Op::NewObj,
                            TIR::Type::obj(oi->className), args,
                            oi->className);
        declareVar(oi->varName, TIR::Type::obj(oi->className),
                   TIR::Val::ofReg(obj, TIR::Type::obj(oi->className)));
        return;
    }

    // ── ArrayAssignment  (arr[i] = val;) ───────────────────────────────────
    if (auto aa = dynamic_cast<const ArrayAssignment*>(stmt)) {
        TIR::Val arr = loadVar(aa->arrayName);
        TIR::Val idx = genExpr(aa->index.get());
        TIR::Val val = genExpr(aa->value.get());
        emitVoid(TIR::Op::StoreArr, val.getType(), {val, arr, idx});
        return;
    }

    // ── Assignment / variable declaration ──────────────────────────────────
    if (auto asgn = dynamic_cast<const Assignment*>(stmt)) {

        // Object-array declaration:  "ClassName[] arr = size_expr;"
        if (!asgn->type.empty() && asgn->type.size() > 2 &&
            asgn->type.substr(asgn->type.size()-2) == "[]") {
            std::string elemType = asgn->type.substr(0, asgn->type.size()-2);
            TIR::Val sizeVal;
            if (asgn->value) sizeVal = genExpr(asgn->value.get());
            else             sizeVal = TIR::Val::constI32(0);
            TIR::Reg arrReg = emit(TIR::Op::NewArray,
                                   TIR::Type::arr(elemType),
                                   {sizeVal}, "", elemType, -1);
            declareVar(asgn->name, TIR::Type::arr(elemType),
                       TIR::Val::ofReg(arrReg, TIR::Type::arr(elemType)));
            return;
        }

        // Class-typed declaration without constructor:  "Person p;"
        if (!asgn->type.empty() && prog_.classes.count(asgn->type) && !asgn->value) {
            TIR::Reg obj = emit(TIR::Op::NewObj, TIR::Type::obj(asgn->type),
                                {}, asgn->type);
            declareVar(asgn->name, TIR::Type::obj(asgn->type),
                       TIR::Val::ofReg(obj, TIR::Type::obj(asgn->type)));
            return;
        }

        // Field assignment through dot:  "obj.field = value"
        if (asgn->name.find('.') != std::string::npos) {
            size_t dot = asgn->name.find('.');
            std::string objPart = asgn->name.substr(0, dot);
            std::string field   = asgn->name.substr(dot + 1);

            TIR::Val val = genExpr(asgn->value.get());

            // "this.field = value"  →  write to the field's alloc slot
            if (objPart == "this") {
                SlotInfo* si = findSlot(field);
                if (si) {
                    emitVoid(TIR::Op::Store, si->type,
                             {val, TIR::Val::ofReg(si->reg, si->type)});
                } else {
                    // Field not in scope (shouldn't happen for valid programs)
                    TIR::Val thisVal = TIR::Val::ofReg(curThisReg_, TIR::Type::obj(curClass_));
                    emitVoid(TIR::Op::StoreField, val.getType(), {val, thisVal}, field);
                }
                return;
            }

            // "arr[idx].field = value"
            size_t lb = objPart.find('[');
            if (lb != std::string::npos) {
                std::string arrName = objPart.substr(0, lb);
                std::string idxStr  = objPart.substr(lb+1, objPart.size()-lb-2);
                TIR::Val arrVal = loadVar(arrName);
                TIR::Val idxVal;
                try { idxVal = TIR::Val::constI32(std::stoi(idxStr)); }
                catch (...) { idxVal = loadVar(idxStr); }
                TIR::Reg elemReg = emit(TIR::Op::LoadArr, TIR::Type::void_(), {arrVal, idxVal});
                TIR::Val elemVal = TIR::Val::ofReg(elemReg, TIR::Type::void_());
                emitVoid(TIR::Op::StoreField, val.getType(), {val, elemVal}, field);
                return;
            }

            // "obj.field = value"
            TIR::Val objVal = loadVar(objPart);
            emitVoid(TIR::Op::StoreField, val.getType(), {val, objVal}, field);
            return;
        }

        // Array literal initialisation:  "int[] arr = {1, 2, 3};"
        if (asgn->value) {
            if (auto al = dynamic_cast<const ArrayLiteral*>(asgn->value.get())) {
                std::vector<TIR::Val> elems;
                for (auto& el : al->elements) elems.push_back(genExpr(el.get()));
                std::string elemType = "int";
                if (!asgn->type.empty() && asgn->type.back() == ']')
                    elemType = asgn->type.substr(0, asgn->type.size()-2);
                else if (!asgn->type.empty())
                    elemType = asgn->type;
                TIR::Reg arrReg = emit(TIR::Op::NewArray,
                                       TIR::Type::arr(elemType),
                                       elems, "", elemType, (int)elems.size());
                if (!asgn->type.empty())
                    declareVar(asgn->name, TIR::Type::arr(elemType),
                               TIR::Val::ofReg(arrReg, TIR::Type::arr(elemType)));
                else
                    storeVar(asgn->name,
                             TIR::Val::ofReg(arrReg, TIR::Type::arr(elemType)));
                return;
            }
        }

        // Ordinary expression RHS (or default-zero for uninitialized declaration)
        TIR::Val rhs;
        if (asgn->value) {
            rhs = genExpr(asgn->value.get());
        } else {
            // Typed declaration without initializer
            TIR::Type ty = tyFromStr(asgn->type);
            if      (ty.isI32() || ty.isI1()) rhs = TIR::Val::constI32(0);
            else if (ty.isF64())              rhs = TIR::Val::constF64(0.0);
            else if (ty.isChar())             rhs = TIR::Val::constChar('\0');
            else if (ty.isStr())              rhs = TIR::Val::constStr("");
            else                              rhs = TIR::Val::constVoid();
        }

        if (!asgn->type.empty()) {
            TIR::Type ty = tyFromStr(asgn->type);
            if (ty.isVoid()) ty = rhs.getType(); // inherit from RHS if type unknown
            declareVar(asgn->name, ty, rhs);
        } else {
            storeVar(asgn->name, rhs);
        }
        return;
    }

    // ── IfStatement ────────────────────────────────────────────────────────
    if (auto ifs = dynamic_cast<const IfStatement*>(stmt)) {
        TIR::Val cond = genExpr(ifs->condition.get());

        std::string thenL  = newLabel("then");
        std::string elseL  = newLabel("else");
        std::string mergeL = newLabel("merge");

        emitTerm(TIR::Term::brCond(cond, thenL, elseL));

        // then block
        addBlock(thenL); switchBlock(thenL);
        pushScope();
        for (auto& s : ifs->thenBranch) genStmt(s.get());
        popScope();
        if (!isSealed()) emitTerm(TIR::Term::br(mergeL));

        // else block
        addBlock(elseL); switchBlock(elseL);
        if (!ifs->elseBranch.empty()) {
            pushScope();
            for (auto& s : ifs->elseBranch) genStmt(s.get());
            popScope();
        }
        if (!isSealed()) emitTerm(TIR::Term::br(mergeL));

        addBlock(mergeL); switchBlock(mergeL);
        return;
    }

    // ── WhileStatement ─────────────────────────────────────────────────────
    if (auto ws = dynamic_cast<const WhileStatement*>(stmt)) {
        std::string headerL = newLabel("while");
        std::string bodyL   = newLabel("wbody");
        std::string exitL   = newLabel("wexit");

        emitTerm(TIR::Term::br(headerL));

        addBlock(headerL); switchBlock(headerL);
        TIR::Val cond = genExpr(ws->condition.get());
        emitTerm(TIR::Term::brCond(cond, bodyL, exitL));

        addBlock(bodyL); switchBlock(bodyL);
        pushScope();
        for (auto& s : ws->body) genStmt(s.get());
        popScope();
        if (!isSealed()) emitTerm(TIR::Term::br(headerL));

        addBlock(exitL); switchBlock(exitL);
        return;
    }

    // ── ForStatement ───────────────────────────────────────────────────────
    if (auto fs = dynamic_cast<const ForStatement*>(stmt)) {
        std::string headerL = newLabel("for");
        std::string bodyL   = newLabel("fbody");
        std::string exitL   = newLabel("fexit");

        pushScope();
        if (fs->initializer) genStmt(fs->initializer.get());

        emitTerm(TIR::Term::br(headerL));
        addBlock(headerL); switchBlock(headerL);

        if (fs->condition) {
            TIR::Val cond = genExpr(fs->condition.get());
            emitTerm(TIR::Term::brCond(cond, bodyL, exitL));
        } else {
            emitTerm(TIR::Term::br(bodyL));
        }

        addBlock(bodyL); switchBlock(bodyL);
        pushScope();
        for (auto& s : fs->body) genStmt(s.get());
        popScope();
        if (fs->increment) genStmt(fs->increment.get());
        if (!isSealed()) emitTerm(TIR::Term::br(headerL));

        addBlock(exitL); switchBlock(exitL);
        popScope();
        return;
    }

    // ── ExprStatement  (call used as statement) ────────────────────────────
    if (auto es = dynamic_cast<const ExprStatement*>(stmt)) {
        genExpr(es->expr.get());
        return;
    }

    throw std::runtime_error("TIRGen: unsupported statement type");
}

// ─────────────────────────────────────────────────────────────────────────────
// Expression code generation  (always returns one TIR::Val)
// ─────────────────────────────────────────────────────────────────────────────

TIR::Val TIRGen::genExpr(const Expr* expr) {

    if (auto n = dynamic_cast<const Number*>(expr))
        return TIR::Val::constI32(n->value);

    if (auto fl = dynamic_cast<const FloatLiteral*>(expr))
        return TIR::Val::constF64(fl->value);

    if (auto ch = dynamic_cast<const CharLiteral*>(expr))
        return TIR::Val::constChar(ch->value);

    if (auto bl = dynamic_cast<const BoolLiteral*>(expr))
        return TIR::Val::constI1(bl->value);

    if (auto sl = dynamic_cast<const StringLiteral*>(expr))
        return TIR::Val::constStr(sl->value);

    if (dynamic_cast<const InputExpr*>(expr)) {
        TIR::Reg r = emit(TIR::Op::Input, TIR::Type::i32());
        return TIR::Val::ofReg(r, TIR::Type::i32());
    }

    if (auto re = dynamic_cast<const ReadExpr*>(expr)) {
        TIR::Reg r = emit(TIR::Op::ReadFile, TIR::Type::i32(), {}, re->filename);
        return TIR::Val::ofReg(r, TIR::Type::i32());
    }

    if (auto v = dynamic_cast<const Variable*>(expr)) {
        if (v->name == "this") {
            TIR::Reg r = emit(TIR::Op::PushThis, TIR::Type::obj(curClass_));
            return TIR::Val::ofReg(r, TIR::Type::obj(curClass_));
        }
        return loadVar(v->name);
    }

    if (auto un = dynamic_cast<const UnaryExpr*>(expr)) {
        TIR::Val src = genExpr(un->operand.get());
        TIR::Type ty = src.getType();
        if (un->op == TokenType::MINUS) {
            TIR::Reg r = emit(TIR::Op::Neg, ty, {src});
            return TIR::Val::ofReg(r, ty);
        }
        if (un->op == TokenType::NOT) {
            TIR::Reg r = emit(TIR::Op::Not, TIR::Type::i1(), {src});
            return TIR::Val::ofReg(r, TIR::Type::i1());
        }
    }

    if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
        TIR::Val lhs = genExpr(bin->left.get());
        TIR::Val rhs = genExpr(bin->right.get());
        TIR::Type lty = lhs.getType();
        TIR::Op   op;
        TIR::Type rty;
        switch (bin->op) {
        case TokenType::PLUS:           op=TIR::Op::Add;   rty=lty;            break;
        case TokenType::MINUS:          op=TIR::Op::Sub;   rty=lty;            break;
        case TokenType::MULTIPLICATION: op=TIR::Op::Mul;   rty=lty;            break;
        case TokenType::DIVISION:       op=TIR::Op::Div;   rty=lty;            break;
        case TokenType::EQUALTO:        op=TIR::Op::CmpEq; rty=TIR::Type::i1();break;
        case TokenType::NOTEQUALTO:     op=TIR::Op::CmpNe; rty=TIR::Type::i1();break;
        case TokenType::LESSTHEN:       op=TIR::Op::CmpLt; rty=TIR::Type::i1();break;
        case TokenType::GREATERTHEN:    op=TIR::Op::CmpGt; rty=TIR::Type::i1();break;
        case TokenType::AND:            op=TIR::Op::And;   rty=TIR::Type::i1();break;
        case TokenType::OR:             op=TIR::Op::Or;    rty=TIR::Type::i1();break;
        default:
            throw std::runtime_error("TIRGen: unsupported binary operator");
        }
        TIR::Reg r = emit(op, lty, {lhs, rhs});
        return TIR::Val::ofReg(r, rty);
    }

    if (auto ce = dynamic_cast<const CastExpr*>(expr)) {
        TIR::Val src = genExpr(ce->operand.get());
        TIR::Op  op;
        TIR::Type ty;
        if      (ce->targetType == "int")   { op=TIR::Op::CastI32;  ty=TIR::Type::i32();  }
        else if (ce->targetType == "float") { op=TIR::Op::CastF64;  ty=TIR::Type::f64();  }
        else if (ce->targetType == "char")  { op=TIR::Op::CastChar; ty=TIR::Type::char_();}
        else if (ce->targetType == "bool")  { op=TIR::Op::CastI1;   ty=TIR::Type::i1();   }
        else                                { op=TIR::Op::CastStr;  ty=TIR::Type::str();  }
        TIR::Reg r = emit(op, ty, {src});
        return TIR::Val::ofReg(r, ty);
    }

    if (auto call = dynamic_cast<const CallExpr*>(expr)) {
        std::vector<TIR::Val> args;
        for (auto& a : call->arguments) args.push_back(genExpr(a.get()));
        TIR::Type retTy = TIR::Type::void_();  // conservative; resolved at runtime
        TIR::Reg r = emit(TIR::Op::Call, retTy, args, call->callee);
        return TIR::Val::ofReg(r, retTy);
    }

    if (auto aa = dynamic_cast<const ArrayAccess*>(expr)) {
        TIR::Val arr = loadVar(aa->arrayName);
        TIR::Val idx = genExpr(aa->index.get());
        TIR::Reg r   = emit(TIR::Op::LoadArr, TIR::Type::void_(), {arr, idx});
        return TIR::Val::ofReg(r, TIR::Type::void_());
    }

    if (auto al = dynamic_cast<const ArrayLiteral*>(expr)) {
        std::vector<TIR::Val> elems;
        for (auto& el : al->elements) elems.push_back(genExpr(el.get()));
        TIR::Reg r = emit(TIR::Op::NewArray, TIR::Type::arr("int"),
                          elems, "", "int", (int)elems.size());
        return TIR::Val::ofReg(r, TIR::Type::arr("int"));
    }

    if (auto oma = dynamic_cast<const ObjectMemberAccess*>(expr)) {
        // "this.field"  →  load from field slot
        if (auto vv = dynamic_cast<const Variable*>(oma->object.get())) {
            if (vv->name == "this") {
                SlotInfo* si = findSlot(oma->member);
                if (si) return loadVar(oma->member);
                // fallback: explicit PushThis + LoadField
                TIR::Reg tr = emit(TIR::Op::PushThis, TIR::Type::obj(curClass_));
                TIR::Reg fr = emit(TIR::Op::LoadField, TIR::Type::void_(),
                                   {TIR::Val::ofReg(tr, TIR::Type::obj(curClass_))},
                                   oma->member);
                return TIR::Val::ofReg(fr, TIR::Type::void_());
            }
        }
        TIR::Val obj = genExpr(oma->object.get());
        TIR::Reg fr  = emit(TIR::Op::LoadField, TIR::Type::void_(), {obj}, oma->member);
        return TIR::Val::ofReg(fr, TIR::Type::void_());
    }

    if (auto omc = dynamic_cast<const ObjectMethodCall*>(expr)) {
        // super.method(args)
        if (auto vv = dynamic_cast<const Variable*>(omc->object.get())) {
            if (vv->name == "super") {
                std::vector<TIR::Val> args;
                for (auto& a : omc->arguments) args.push_back(genExpr(a.get()));
                TIR::Reg r = emit(TIR::Op::CallSuper, TIR::Type::void_(),
                                  args, omc->method);
                return TIR::Val::ofReg(r, TIR::Type::void_());
            }
        }
        // Regular obj.method(args)
        TIR::Val obj = genExpr(omc->object.get());
        std::vector<TIR::Val> args;
        for (auto& a : omc->arguments) args.push_back(genExpr(a.get()));
        // args: [obj, arg0, arg1, ...]
        std::vector<TIR::Val> allArgs = {obj};
        allArgs.insert(allArgs.end(), args.begin(), args.end());
        TIR::Reg r = emit(TIR::Op::CallMethod, TIR::Type::void_(),
                          allArgs, omc->method);
        return TIR::Val::ofReg(r, TIR::Type::void_());
    }

    throw std::runtime_error("TIRGen: unsupported expression type");
}

// ─────────────────────────────────────────────────────────────────────────────
// Top-level entry point
// ─────────────────────────────────────────────────────────────────────────────

TIR::Program TIRGen::generate(const std::vector<std::unique_ptr<Statement>>& stmts) {
    prog_        = {};
    labelCount_  = 0;
    curClass_.clear();
    curParams_.clear();
    curAllFields_.clear();
    curThisReg_  = TIR::NOREG;
    scopes_.clear();

    // Pass 1: compile all class/function definitions
    firstPass(stmts);

    // Pass 2: compile top-level (main) code
    TIR::Func& gi  = prog_.globalInit;
    gi.name        = "__global__";
    curFunc_       = &gi;
    curBlockIdx_   = -1;
    curClass_.clear();
    curThisReg_    = TIR::NOREG;
    curAllFields_.clear();
    scopes_.clear();

    addBlock("entry"); switchBlock("entry");
    pushScope();

    for (auto& st : stmts) {
        if (dynamic_cast<const ClassDef*>(st.get()))       continue;
        if (dynamic_cast<const FunctionDef*>(st.get()))    continue;
        if (dynamic_cast<const ImportStatement*>(st.get())) continue;
        genStmt(st.get());
    }

    if (!isSealed()) emitTerm(TIR::Term::ret());
    popScope();

    return prog_;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public wrappers
// ─────────────────────────────────────────────────────────────────────────────

TIR::Program generateTIR(const std::vector<std::unique_ptr<Statement>>& stmts) {
    TIRGen gen;
    return gen.generate(stmts);
}

void dumpTIR(const TIR::Program& prog) {
    TIR::dumpProgram(prog);
}
