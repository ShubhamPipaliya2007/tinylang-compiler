#pragma once
#include "tir.hpp"
#include "object.hpp"

#include <string>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------------------------
// TIRVM – interpreter for register-based TIR::Program.
//
// Execution model (Phase 4.3 — pointer-based object layout):
//   • Each call frame has a register file (regs) and an alloc-slot map (mem).
//     Both use TLValue — the typed 8-byte value word.
//   • Objects are TLObject*; arrays are TLArray*.  No string handles.
//   • alloc %r "name"     → mem[r] = default
//   • load  %r %slot      → regs[r] = mem[slot]
//   • store %val -> %slot → mem[slot] = eval(val)
//   • Field access uses TLObject::getField/setField (name→index lookup).
//   • TLHeap owns all objects for the program's lifetime.
// ---------------------------------------------------------------------------

struct TIRFrame {
    const TIR::Func*                          func     = nullptr;
    std::unordered_map<TIR::Reg, TLValue>     regs;    // virtual register file
    std::unordered_map<TIR::Reg, TLValue>     mem;     // alloc-slot cells
    std::string                               className;
    TLObject*                                 thisObj  = nullptr;
};

class TIRVM {
public:
    void run(const TIR::Program& prog);

private:
    const TIR::Program*  prog_ = nullptr;
    TLHeap               heap_;

    std::unordered_map<std::string, const TIR::Func*> methodCache_;

    // Evaluate a Val operand (register ref or inline constant → TLValue).
    TLValue evalVal(const TIR::Val& v, const TIRFrame& frame) const;

    // Execute one function; returns its return value.
    TLValue callFunc(const std::string& funcKey,
                     const std::vector<TLValue>& args,
                     TLObject* thisObj  = nullptr,
                     const std::string& className = "");

    struct ExecResult {
        enum Kind { Ret, RetVal, Br, BrCond } kind;
        TLValue     val;
        std::string target;
        bool        condTaken = false;
    };
    ExecResult execBlock(const TIR::Block& block,
                         TIRFrame& frame,
                         const std::vector<TLValue>& callArgs);

    // Object / class helpers
    void collectAllFields(const std::string& cls,
                          std::vector<std::pair<TIR::Type, std::string>>& out) const;
    void initObjectFields(const std::string& cls, TLObject* obj);
    // Re-sync field alloc-slots in frame from the object's field vector
    // after a super/method call that may have mutated shared fields.
    void resyncFieldSlots(TIRFrame& frame, TLObject* obj);

    const TIR::Func* findMethod(const std::string& cls,
                                const std::string& method) const;
    const TIR::Func* findMethodCached(const std::string& cls,
                                      const std::string& method);

    // Value helpers
    std::string valueToString(const TLValue& v) const;
    TLValue arith  (const TLValue& l, const TLValue& r, TIR::Op op) const;
    TLValue compare(const TLValue& l, const TLValue& r, TIR::Op op) const;

    static TLValue defaultValue(TIR::Type ty);
};

void runTIR(const TIR::Program& prog);
