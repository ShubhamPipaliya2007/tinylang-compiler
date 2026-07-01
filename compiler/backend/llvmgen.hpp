#pragma once
#include "tir.hpp"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <sstream>

// ---------------------------------------------------------------------------
// LLVMGen — lowers TIR::Program to LLVM textual IR (`.ll`).
//
// Compile the output with:
//   clang output.ll runtime/native/tinyrt.c -o program
//
// Design notes:
//   • No libLLVM dependency; emits `.ll` text directly.
//   • Uses opaque pointers (`ptr`) — requires LLVM/clang 15+.
//   • alloc → alloca,  load/store → load/store,  call → call.
//   • TIRGen emits void return types on functions that actually return values
//     (a known type-inference gap).  LLVMGen scans each function body for
//     RetVal terminators to recover the true return type before emission.
//   • Comparison results are i1 in LLVM IR; BrCond conditions are emitted
//     directly as i1.  Arithmetic on comparison results uses zext.
// ---------------------------------------------------------------------------

class LLVMGen {
public:
    // Emit LLVM IR for the entire program.
    // Returns the full `.ll` text.
    std::string emit(const TIR::Program& prog);

private:
    const TIR::Program*  prog_ = nullptr;
    std::ostringstream   out_;

    // ── Per-function state, reset for each function ────────────────────────
    // Logical LLVM type of each virtual register.
    std::unordered_map<TIR::Reg, TIR::Type> regTypes_;
    // Registers produced by Alloc are alloca slots; referenced as `ptr`.
    std::unordered_set<TIR::Reg> allocSlots_;
    // Param registers: reg → positional index (references %argN).
    std::unordered_map<TIR::Reg, int> paramRegs_;

    // ── Program-level state ────────────────────────────────────────────────
    // Inferred actual return types (recovers from TIRGen void-return bug).
    std::unordered_map<std::string, TIR::Type> funcRetTypes_;
    // String constant literals → global name (@.str.N).
    std::unordered_map<std::string, std::string> strGlobals_;
    int strCounter_ = 0;
    // Per-function counter for collision-free temporary names.
    int tmpCounter_ = 0;

    // ── Emission passes ───────────────────────────────────────────────────
    void buildRetTypeMap();
    void collectStrings(const TIR::Func& fn);
    void emitStringGlobals();
    void emitRuntimeDecls();
    void emitFunc(const TIR::Func& fn, bool isMain);
    void emitBlock(const TIR::Block& blk, const TIR::Func& fn,
                   bool isMain, const TIR::Type& retTy);
    void emitInstr(const TIR::Instr& ins);
    void emitTerm(const TIR::Term& term, bool isMain, const TIR::Type& retTy);

    // Resolve the field index of fieldName in the class of objVal.
    int resolveFieldIndex(const TIR::Val& objVal,
                          const std::string& fieldName) const;

    // Unique scratch register name (cannot collide with %rN user regs).
    std::string tmp(const std::string& tag);

    // ── Type helpers ──────────────────────────────────────────────────────
    // Convert TIR::Type to LLVM type string.
    std::string llvmType(const TIR::Type& ty) const;
    // Alignment in bytes for a type (used in alloca/load/store).
    int alignOf(const TIR::Type& ty) const;
    // Recovered type of a Val: checks regTypes_ before falling back to v.type.
    TIR::Type effectiveType(const TIR::Val& v) const;
    // Infer actual return type of fn by scanning RetVal terminators.
    TIR::Type inferRetType(const TIR::Func& fn) const;
    // Return type to use when calling a function (looks up funcRetTypes_).
    TIR::Type callRetType(const std::string& funcName) const;

    // ── Value / name helpers ──────────────────────────────────────────────
    // Emit an operand (register ref or constant literal).
    std::string llvmVal(const TIR::Val& v) const;
    // Emit "type operand" pair.
    std::string llvmTypedVal(const TIR::Val& v) const;
    // Register reference string (%argN for params, %rN for others).
    std::string regRef(TIR::Reg r) const;
    // LLVM symbol name for a TinyLang function.
    std::string funcSym(const std::string& name, const std::string& cls = "") const;
    // Exact hex float literal for a double (avoids decimal rounding).
    static std::string hexFloat(double d);
};

// Compile-time entry point: returns LLVM IR text.
std::string emitLLVM(const TIR::Program& prog);
