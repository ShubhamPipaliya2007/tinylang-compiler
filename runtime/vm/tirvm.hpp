#pragma once
#include "tir.hpp"
#include "irvm.hpp"   // reuse IRValue, IRObject, IRArray runtime types

#include <string>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------------------------
// TIRVM – interpreter for register-based TIR::Program.
//
// Execution model:
//   • Each call frame has a register file (regs) for computed values and a
//     memory map (mem) for alloc-slot cells.
//   • alloc %r "name"      → creates mem[r] = default
//   • load  %r %slot       → regs[r] = mem[slot]
//   • store %val -> %slot  → mem[slot] = eval(val)
//   • Other instructions compute into regs[dest].
//   • Control flow follows explicit terminators (Ret, RetVal, Br, BrCond).
//   • Object/array heap is shared across all frames (same as IRVM).
// ---------------------------------------------------------------------------

struct TIRFrame {
    const TIR::Func*                              func      = nullptr;
    std::unordered_map<TIR::Reg, IRValue>         regs;   // computed register values
    std::unordered_map<TIR::Reg, IRValue>         mem;    // alloc-slot cells
    std::string                                   className;
    std::string                                   thisHandle;
};

class TIRVM {
public:
    void run(const TIR::Program& prog);

private:
    const TIR::Program*                          prog_ = nullptr;
    std::unordered_map<std::string, IRObject>    objHeap_;
    std::unordered_map<std::string, IRArray>     arrHeap_;
    int                                          handleCounter_ = 0;

    std::unordered_map<std::string, const TIR::Func*> methodCache_;

    std::string newHandle(const std::string& pfx = "h");

    // Evaluate a Val operand (register or inline constant → IRValue).
    IRValue evalVal(const TIR::Val& v, const TIRFrame& frame) const;

    // Execute one function; returns its return value.
    IRValue callFunc(const std::string& funcKey,
                     const std::vector<IRValue>& args,
                     const std::string& thisHandle = "",
                     const std::string& className  = "");

    // Execute a single basic block; returns (terminatorKind, payload).
    // The caller follows the terminator to decide the next block.
    struct ExecResult {
        enum Kind { Ret, RetVal, Br, BrCond } kind;
        IRValue  val;
        std::string target;
        bool condTaken = false;
    };
    ExecResult execBlock(const TIR::Block& block,
                         TIRFrame& frame,
                         const std::vector<IRValue>& callArgs);

    // Object / class helpers (mirrors IRVM logic)
    void collectAllFields(const std::string& cls,
                          std::vector<std::pair<std::string,std::string>>& out) const;

    // Re-sync field alloc-slots in frame from heap after a super/method call
    // that may have mutated the shared object.
    void resyncFieldSlots(TIRFrame& frame, const std::string& objHandle);
    void initObjectFields(const std::string& cls, IRObject& obj);
    const TIR::Func* findMethod(const std::string& cls, const std::string& method) const;
    const TIR::Func* findMethodCached(const std::string& cls, const std::string& method);

    // Arithmetic / comparison helpers (same semantics as IRVM)
    std::string valueToString(const IRValue& v) const;
    IRValue arith  (const IRValue& l, const IRValue& r, TIR::Op op) const;
    IRValue compare(const IRValue& l, const IRValue& r, TIR::Op op) const;

    static IRValue defaultValue(TIR::Type ty);
};

void runTIR(const TIR::Program& prog);
