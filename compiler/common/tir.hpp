#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

// ---------------------------------------------------------------------------
// TinyIR – typed, register-based IR designed to map cleanly to LLVM IR.
//
// Design principles:
//   • Virtual registers  %N  replace the operand stack.
//   • Every value is typed  (i1 · i32 · f64 · char · str · obj<C> · arr<T>).
//   • Basic blocks are first-class; exactly one terminator closes each block.
//   • Variable storage uses named alloc-slots (≈ LLVM alloca / mem2reg).
//   • Phi nodes are reserved for the optimizer; the generator emits alloc+load+store.
// ---------------------------------------------------------------------------

namespace TIR {

// ─── Type system ──────────────────────────────────────────────────────────────
enum class BaseType { Void, I1, I32, F64, Char, Str, ObjRef, ArrRef };

struct Type {
    BaseType    base = BaseType::Void;
    std::string name;   // class name for ObjRef; element-type name for ArrRef

    bool isVoid()    const { return base == BaseType::Void; }
    bool isI1()      const { return base == BaseType::I1;   }
    bool isI32()     const { return base == BaseType::I32;  }
    bool isF64()     const { return base == BaseType::F64;  }
    bool isChar()    const { return base == BaseType::Char; }
    bool isStr()     const { return base == BaseType::Str;  }
    bool isObj()     const { return base == BaseType::ObjRef; }
    bool isArr()     const { return base == BaseType::ArrRef; }
    bool isNumeric() const { return isI32() || isF64(); }

    bool operator==(const Type& o) const { return base==o.base && name==o.name; }
    bool operator!=(const Type& o) const { return !(*this==o); }

    std::string asStr() const;   // human-readable type name

    static Type void_()            { return {BaseType::Void, ""}; }
    static Type i1()               { return {BaseType::I1,   ""}; }
    static Type i32()              { return {BaseType::I32,  ""}; }
    static Type f64()              { return {BaseType::F64,  ""}; }
    static Type char_()            { return {BaseType::Char, ""}; }
    static Type str()              { return {BaseType::Str,  ""}; }
    static Type obj(std::string c) { return {BaseType::ObjRef, std::move(c)}; }
    static Type arr(std::string e) { return {BaseType::ArrRef, std::move(e)}; }
};

// ─── Virtual register ─────────────────────────────────────────────────────────
using Reg = uint32_t;
constexpr Reg NOREG = ~0u;

// ─── Typed value operand ──────────────────────────────────────────────────────
// A Val is either a virtual-register reference or an inline constant.
struct Val {
    Reg         reg  = NOREG;   // NOREG ⇒ inline constant
    Type        type;
    int         ival = 0;
    double      dval = 0.0;
    char        cval = 0;
    std::string sval;

    bool isReg()   const { return reg != NOREG; }
    bool isConst() const { return reg == NOREG; }
    Type getType() const { return type; }
    std::string asStr() const;  // human-readable operand

    static Val ofReg(Reg r, Type t)      { Val v; v.reg=r; v.type=t; return v; }
    static Val constI32(int i)           { Val v; v.type=Type::i32();   v.ival=i; return v; }
    static Val constF64(double d)        { Val v; v.type=Type::f64();   v.dval=d; return v; }
    static Val constI1(bool b)           { Val v; v.type=Type::i1();    v.ival=b?1:0; return v; }
    static Val constChar(char c)         { Val v; v.type=Type::char_(); v.cval=c; return v; }
    static Val constStr(std::string s)   { Val v; v.type=Type::str();   v.sval=std::move(s); return v; }
    static Val constVoid()               { Val v; v.type=Type::void_(); return v; }
};

// ─── Non-terminator opcodes ───────────────────────────────────────────────────
enum class Op {
    // Variable / memory (alloca+load+store pattern)
    Alloc,       // %r = alloc type "name"        — named mutable slot
    Load,        // %r = load type %slot          — read from slot
    Store,       // store type %val -> %slot      — write to slot  (no dest)

    // Parameter reference (emitted at function entry for each param)
    ParamRef,    // %r = param N                  — value of the Nth argument

    // Arithmetic
    Add, Sub, Mul, Div,
    Neg,

    // Comparison  → i1
    CmpEq, CmpNe, CmpLt, CmpGt,

    // Logical
    And, Or, Not,

    // Casts
    CastI32, CastF64, CastChar, CastI1, CastStr,

    // I/O
    Print,       // print type %val               — no dest
    Input,       // %r = input                    — reads i32 from stdin
    ReadFile,    // %r = read.file "filename"

    // Free-function call
    Call,        // %r = call type "name"(args...)

    // Object / OOP
    PushThis,    // %r = this                     — current method's receiver
    NewObj,      // %r = new.obj "ClassName"(args...)
    LoadField,   // %r = load.field type %obj "field"
    StoreField,  // store.field type %val -> %obj "field"  — no dest
    CallMethod,  // %r = call.method type "method" %obj(args...)
    CallSuper,   // %r = call.super  type "method"(args...)

    // Arrays
    NewArray,    // %r = new.arr "elemType" ival  (ival<0 ⇒ size from args[0])
    LoadArr,     // %r = load.arr type %arr %idx
    StoreArr,    // store.arr type %val -> %arr %idx        — no dest

    // SSA phi (inserted by optimizer; not emitted by front-end)
    Phi,

    Nop,
};

// ─── Instruction ──────────────────────────────────────────────────────────────
struct PhiSrc { Val val; std::string predLabel; };

struct Instr {
    Reg                  dest  = NOREG;
    Op                   op    = Op::Nop;
    Type                 type;
    std::vector<Val>     args;
    std::string          name;   // variable/func/field/method/filename
    std::string          name2;  // class (CallMethod/NewObj), elem type (NewArray)
    int                  ival  = 0;  // param index (ParamRef), elem count (NewArray; -1=stack)
    std::vector<PhiSrc>  phi;        // only for Op::Phi
};

// ─── Block terminator ─────────────────────────────────────────────────────────
enum class TermKind { Ret, RetVal, Br, BrCond };

struct Term {
    TermKind    kind        = TermKind::Ret;
    Val         val;            // RetVal payload
    std::string target;         // Br
    Val         cond;           // BrCond condition
    std::string trueTarget;     // BrCond taken branch
    std::string falseTarget;    // BrCond not-taken branch

    static Term ret()                                          { Term t; t.kind=TermKind::Ret; return t; }
    static Term retVal(Val v)                                  { Term t; t.kind=TermKind::RetVal; t.val=v; return t; }
    static Term br(std::string lbl)                            { Term t; t.kind=TermKind::Br; t.target=std::move(lbl); return t; }
    static Term brCond(Val c, std::string tl, std::string fl) { Term t; t.kind=TermKind::BrCond; t.cond=c; t.trueTarget=std::move(tl); t.falseTarget=std::move(fl); return t; }
};

// ─── Basic block ──────────────────────────────────────────────────────────────
struct Block {
    std::string        label;
    std::vector<Instr> instrs;
    Term               term;
    bool               sealed = false;  // true once term is set
};

// ─── Function / method ────────────────────────────────────────────────────────
struct Func {
    std::string name;
    std::string className;                              // empty for free functions
    Type        retType = Type::void_();
    std::vector<std::pair<Type, std::string>> params;  // (type, paramName)
    std::vector<Block> blocks;
    Reg nextReg = 0;

    Reg freshReg() { return nextReg++; }
};

// ─── Class descriptor ─────────────────────────────────────────────────────────
struct Class {
    std::string name;
    std::string baseClass;
    std::vector<std::pair<Type, std::string>> fields;
};

// ─── Program ──────────────────────────────────────────────────────────────────
struct Program {
    std::unordered_map<std::string, Class> classes;
    std::unordered_map<std::string, Func>  funcs;
    Func                                   globalInit;  // top-level statements
};

// ─── Pretty-printer ───────────────────────────────────────────────────────────
void dumpProgram(const Program& prog);
void dumpFunc(const Func& fn);

} // namespace TIR
