#pragma once
#include "tir.hpp"
#include "ast.hpp"
#include <unordered_map>
#include <set>

// ---------------------------------------------------------------------------
// TIRGen – lowers TinyLang AST to TIR::Program.
//
// Code-gen model:
//   • Expressions return TIR::Val (register ref or inline constant).
//   • Statements control flow by splitting the current function into
//     basic blocks and wiring them with explicit terminators.
//   • All mutable locals are alloc-slots (ParamRef → Alloc+Store at entry;
//     bare reads/writes → Load/Store through the slot).
//   • For class methods the generator emits a field-init prologue
//     (PushThis + LoadField → slot) and a field-sync epilogue before each ret.
// ---------------------------------------------------------------------------

class TIRGen {
public:
    TIR::Program generate(const std::vector<std::unique_ptr<Statement>>& stmts);

private:
    // ── Generator state ────────────────────────────────────────────────────
    TIR::Program  prog_;
    TIR::Func*    curFunc_     = nullptr;
    int           curBlockIdx_ = -1;     // index into curFunc_->blocks
    int           labelCount_  = 0;
    std::string   curClass_;
    std::set<std::string>                          curParams_;
    std::vector<std::pair<TIR::Type, std::string>> curAllFields_;  // (tirType, name)
    TIR::Reg      curThisReg_  = TIR::NOREG;  // reg holding "this" in methods

    // Scope stack: varName → (slotReg, slotType)
    struct SlotInfo { TIR::Reg reg; TIR::Type type; };
    std::vector<std::unordered_map<std::string, SlotInfo>> scopes_;

    // ── Block helpers ──────────────────────────────────────────────────────
    std::string newLabel(const std::string& pfx);
    TIR::Reg    freshReg();
    TIR::Block& curBlock();
    bool        isSealed() const;

    std::string addBlock(const std::string& label);   // adds block, returns label
    void        switchBlock(const std::string& label); // sets curBlockIdx_

    // ── Emit helpers ───────────────────────────────────────────────────────
    // Returns the dest register (freshly allocated).
    TIR::Reg  emit(TIR::Op op, TIR::Type ty,
                   std::vector<TIR::Val> args = {},
                   std::string name  = "",
                   std::string name2 = "",
                   int ival = 0);
    // Void variant (no dest register).
    void      emitVoid(TIR::Op op, TIR::Type ty,
                       std::vector<TIR::Val> args = {},
                       std::string name  = "",
                       std::string name2 = "",
                       int ival = 0);
    void      emitTerm(TIR::Term t);

    // ── Scope / variable helpers ───────────────────────────────────────────
    void pushScope();
    void popScope();

    SlotInfo*  findSlot(const std::string& name);
    TIR::Reg   declareVar(const std::string& name, TIR::Type ty, TIR::Val init);
    TIR::Val   loadVar(const std::string& name);
    void       storeVar(const std::string& name, TIR::Val val);

    // ── Type helpers ───────────────────────────────────────────────────────
    TIR::Type tyFromStr(const std::string& s) const;
    TIR::Type inferType(const Expr* e) const;

    // ── Method field helpers ───────────────────────────────────────────────
    void collectAllFields(const std::string& cls,
                          std::vector<std::pair<TIR::Type,std::string>>& out) const;
    void emitFieldSync();  // emit PushThis+StoreField for all curAllFields_

    // ── Compilation passes ─────────────────────────────────────────────────
    void firstPass(const std::vector<std::unique_ptr<Statement>>& stmts);
    void compileFunction(const FunctionDef* fn, const std::string& cls = "");

    void     genStmt(const Statement* stmt);
    TIR::Val genExpr(const Expr* expr);
};

TIR::Program generateTIR(const std::vector<std::unique_ptr<Statement>>& stmts);
void         dumpTIR(const TIR::Program& prog);
