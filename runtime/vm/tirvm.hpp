#pragma once
#include "tir.hpp"
#include "object.hpp"

#include <string>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------------------------
// TIRVM – interpreter for register-based TIR::Program.
//
// Execution model (Phase 4.4 — pointer-based layout + mark-and-sweep GC):
//   • Each call frame has a register file (regs) and an alloc-slot map (mem).
//     Both use TLValue — the typed 8-byte value word.
//   • Objects are TLObject*; arrays are TLArray*.  No string handles.
//   • callStack_ tracks every active TIRFrame* as GC roots.
//   • After each NewObj/NewArray, if heap_.shouldCollect(), runGC() fires:
//       1. Mark: walk callStack_, mark every TLObject*/TLArray* reachable.
//       2. Sweep: heap_.sweep() deletes unmarked objects, clears mark bits.
//   • FrameGuard (RAII) maintains callStack_ across all call paths.
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
    const TIR::Program*      prog_ = nullptr;
    TLHeap                   heap_;
    std::vector<TIRFrame*>   callStack_;   // all active frames — GC root set

    std::unordered_map<std::string, const TIR::Func*> methodCache_;

    // RAII guard: pushes frame on entry, pops on any exit (return or exception).
    struct FrameGuard {
        std::vector<TIRFrame*>& stack;
        FrameGuard(std::vector<TIRFrame*>& s, TIRFrame* f) : stack(s) { s.push_back(f); }
        ~FrameGuard() { stack.pop_back(); }
    };

    // ── GC ────────────────────────────────────────────────────────────────
    // Mark all roots reachable from callStack_, then sweep the heap.
    void runGC();

    // ── Native function dispatch ──────────────────────────────────────────
    // Called by callFunc() when funcKey starts with "__tl_".
    // Implements built-in string, array, and file operations in C++
    // so the interpreter path works without linking tinyrt.c.
    TLValue callNative(const std::string& name, const std::vector<TLValue>& args);

    // ── Evaluation ────────────────────────────────────────────────────────
    TLValue evalVal(const TIR::Val& v, const TIRFrame& frame) const;

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

    // ── Object / class helpers ────────────────────────────────────────────
    void collectAllFields(const std::string& cls,
                          std::vector<std::pair<TIR::Type, std::string>>& out) const;
    void initObjectFields(const std::string& cls, TLObject* obj);
    void resyncFieldSlots(TIRFrame& frame, TLObject* obj);

    const TIR::Func* findMethod(const std::string& cls,
                                const std::string& method) const;
    const TIR::Func* findMethodCached(const std::string& cls,
                                      const std::string& method);

    // ── Value helpers ─────────────────────────────────────────────────────
    std::string valueToString(const TLValue& v) const;
    TLValue arith  (const TLValue& l, const TLValue& r, TIR::Op op) const;
    TLValue compare(const TLValue& l, const TLValue& r, TIR::Op op) const;

    static TLValue defaultValue(TIR::Type ty);
};

void runTIR(const TIR::Program& prog);
