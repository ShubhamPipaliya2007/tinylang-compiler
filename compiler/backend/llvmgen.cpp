#include "llvmgen.hpp"
#include <cstdint>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <stdexcept>
#include <algorithm>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::string escapeLLVMStr(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 8);
    for (unsigned char c : s) {
        if      (c == '\\') r += "\\\\";
        else if (c == '"')  r += "\\22";
        else if (c == '\n') r += "\\0A";
        else if (c == '\r') r += "\\0D";
        else if (c == '\t') r += "\\09";
        else if (c < 32 || c > 126) {
            char buf[5]; snprintf(buf, sizeof(buf), "\\%02X", c); r += buf;
        } else r += (char)c;
    }
    return r;
}

// IEEE 754 hex literal (avoids decimal rounding in the .ll file).
std::string LLVMGen::hexFloat(double d) {
    uint64_t bits;
    memcpy(&bits, &d, 8);
    char buf[32];
    snprintf(buf, sizeof(buf), "0x%016llX", (unsigned long long)bits);
    return buf;
}

// ---------------------------------------------------------------------------
// Type helpers
// ---------------------------------------------------------------------------

std::string LLVMGen::llvmType(const TIR::Type& ty) const {
    switch (ty.base) {
    case TIR::BaseType::Void:    return "void";
    case TIR::BaseType::I1:      return "i1";
    case TIR::BaseType::I32:     return "i32";
    case TIR::BaseType::F64:     return "double";
    case TIR::BaseType::Char:    return "i32";
    case TIR::BaseType::Str:     return "ptr";
    case TIR::BaseType::ObjRef:  return "ptr";
    case TIR::BaseType::ArrRef:  return "ptr";
    }
    return "void";
}

int LLVMGen::alignOf(const TIR::Type& ty) const {
    if (ty.isF64())                         return 8;
    if (ty.isStr() || ty.isObj() || ty.isArr()) return 8;
    return 4;
}

TIR::Type LLVMGen::effectiveType(const TIR::Val& v) const {
    if (v.isReg()) {
        auto it = regTypes_.find(v.reg);
        if (it != regTypes_.end() && !it->second.isVoid())
            return it->second;
    }
    return v.type;
}

TIR::Type LLVMGen::inferRetType(const TIR::Func& fn) const {
    if (!fn.retType.isVoid()) return fn.retType;
    for (auto& blk : fn.blocks) {
        if (blk.term.kind == TIR::TermKind::RetVal)
            return blk.term.val.type;
    }
    return TIR::Type::void_();
}

TIR::Type LLVMGen::callRetType(const std::string& key) const {
    auto it = funcRetTypes_.find(key);
    if (it != funcRetTypes_.end()) return it->second;
    return TIR::Type::void_();
}

// ---------------------------------------------------------------------------
// Value / name helpers
// ---------------------------------------------------------------------------

std::string LLVMGen::regRef(TIR::Reg r) const {
    auto it = paramRegs_.find(r);
    if (it != paramRegs_.end())
        return "%arg" + std::to_string(it->second);
    return "%r" + std::to_string(r);
}

std::string LLVMGen::llvmVal(const TIR::Val& v) const {
    if (v.isReg()) return regRef(v.reg);
    if (v.type.isI32() || v.type.isI1()) return std::to_string(v.ival);
    if (v.type.isChar())                  return std::to_string((unsigned char)v.cval);
    if (v.type.isF64())                   return hexFloat(v.dval);
    if (v.type.isStr()) {
        auto it = strGlobals_.find(v.sval);
        if (it != strGlobals_.end()) return "@" + it->second;
        return "null";
    }
    return "undef";
}

std::string LLVMGen::llvmTypedVal(const TIR::Val& v) const {
    return llvmType(effectiveType(v)) + " " + llvmVal(v);
}

std::string LLVMGen::funcSym(const std::string& name,
                              const std::string& cls) const {
    if (name == "__global__") return "main";
    if (!cls.empty())
        return "_TL_" + cls + "__" + name;
    auto sep = name.find("::");
    if (sep != std::string::npos)
        return "_TL_" + name.substr(0, sep) + "__" + name.substr(sep + 2);
    return name;
}

// Unique temporary name that cannot collide with %rN register names.
std::string LLVMGen::tmp(const std::string& tag) {
    return "%_" + tag + "_" + std::to_string(tmpCounter_++);
}

// ---------------------------------------------------------------------------
// Pre-pass
// ---------------------------------------------------------------------------

// Native runtime functions visible to both interpreter and LLVM paths.
static const std::unordered_map<std::string, TIR::Type>& nativeRetTypes() {
    static const std::unordered_map<std::string, TIR::Type> t = {
        // print (void)
        {"__tl_print_i32",    TIR::Type::void_()},
        {"__tl_print_f64",    TIR::Type::void_()},
        {"__tl_print_str",    TIR::Type::void_()},
        {"__tl_print_bool",   TIR::Type::void_()},
        {"__tl_print_char",   TIR::Type::void_()},
        // input
        {"__tl_input_i32",    TIR::Type::i32()},
        // string → string
        {"__tl_str_concat",   TIR::Type::str()},
        {"__tl_i32_to_str",   TIR::Type::str()},
        {"__tl_f64_to_str",   TIR::Type::str()},
        {"__tl_bool_to_str",  TIR::Type::str()},
        {"__tl_str_sub",      TIR::Type::str()},
        {"__tl_str_upper",    TIR::Type::str()},
        {"__tl_str_lower",    TIR::Type::str()},
        {"__tl_str_trim",     TIR::Type::str()},
        {"__tl_str_replace",  TIR::Type::str()},
        // string → int
        {"__tl_str_len",        TIR::Type::i32()},
        {"__tl_str_eq",         TIR::Type::i32()},
        {"__tl_str_contains",   TIR::Type::i32()},
        {"__tl_str_starts_with",TIR::Type::i32()},
        {"__tl_str_ends_with",  TIR::Type::i32()},
        {"__tl_str_index_of",   TIR::Type::i32()},
        {"__tl_str_char_at",    TIR::Type::i32()},
        {"__tl_str_to_int",     TIR::Type::i32()},
        // string → float
        {"__tl_str_to_float",   TIR::Type::f64()},
        // array
        {"__tl_alloc_arr",  TIR::Type::arr("any")},
        {"__tl_arr_len",    TIR::Type::i32()},
        {"__tl_load_arr",   TIR::Type::i32()},   // raw i64 → i32 for int arrays
        {"__tl_store_arr",  TIR::Type::void_()},
        {"__tl_arr_resize", TIR::Type::arr("any")},
        // file
        {"__tl_file_exists",    TIR::Type::i32()},
        {"__tl_file_read_all",  TIR::Type::str()},
        {"__tl_file_write_all", TIR::Type::i32()},
        {"__tl_file_append",    TIR::Type::i32()},
        {"__tl_file_delete",    TIR::Type::i32()},
    };
    return t;
}

void LLVMGen::buildRetTypeMap() {
    for (auto& [name, ty] : nativeRetTypes())
        funcRetTypes_[name] = ty;
    for (auto& [k, fn] : prog_->funcs)
        funcRetTypes_[k] = inferRetType(fn);
    funcRetTypes_["__global__"] = inferRetType(prog_->globalInit);
}

void LLVMGen::collectStrings(const TIR::Func& fn) {
    auto maybeStr = [&](const TIR::Val& v) {
        if (!v.isConst() || !v.type.isStr()) return;
        if (strGlobals_.count(v.sval)) return;
        strGlobals_[v.sval] = ".str." + std::to_string(strCounter_++);
    };
    for (auto& blk : fn.blocks)
        for (auto& ins : blk.instrs)
            for (auto& a : ins.args)
                maybeStr(a);
}

// ---------------------------------------------------------------------------
// Module-level emission
// ---------------------------------------------------------------------------

void LLVMGen::emitStringGlobals() {
    std::vector<std::pair<std::string, std::string>> entries(
        strGlobals_.begin(), strGlobals_.end());
    std::sort(entries.begin(), entries.end(),
              [](auto& a, auto& b) { return a.second < b.second; });
    for (auto& [s, gname] : entries) {
        out_ << "@" << gname << " = private unnamed_addr constant ["
             << (s.size() + 1) << " x i8] c\""
             << escapeLLVMStr(s) << "\\00\", align 1\n";
    }
    if (!strGlobals_.empty()) out_ << "\n";
}

void LLVMGen::emitRuntimeDecls() {
    out_ << "; ── TinyLang runtime ─────────────────────────────────────────\n";
    out_ << "declare void @__tl_print_i32(i32)\n";
    out_ << "declare void @__tl_print_f64(double)\n";
    out_ << "declare void @__tl_print_bool(i32)\n";
    out_ << "declare void @__tl_print_char(i32)\n";
    out_ << "declare void @__tl_print_str(ptr)\n";
    out_ << "declare i32  @__tl_input_i32()\n";
    out_ << "declare ptr  @__tl_str_concat(ptr, ptr)\n";
    out_ << "declare ptr  @__tl_i32_to_str(i32)\n";
    out_ << "declare ptr  @__tl_f64_to_str(double)\n";
    out_ << "declare ptr  @__tl_bool_to_str(i32)\n";
    out_ << "declare i32  @__tl_str_eq(ptr, ptr)\n";
    out_ << "declare i32  @__tl_str_len(ptr)\n";
    out_ << "declare ptr  @__tl_alloc_obj(i32)\n";
    out_ << "declare i64  @__tl_load_field(ptr, i32)\n";
    out_ << "declare void @__tl_store_field(ptr, i32, i64)\n";
    out_ << "declare ptr  @__tl_alloc_arr(i32)\n";
    out_ << "declare i32  @__tl_arr_len(ptr)\n";
    out_ << "declare i64  @__tl_load_arr(ptr, i32)\n";
    out_ << "declare void @__tl_store_arr(ptr, i32, i64)\n";
    out_ << "declare ptr  @__tl_arr_resize(ptr, i32)\n";
    out_ << "declare i32  @__tl_str_len(ptr)\n";
    out_ << "declare ptr  @__tl_str_sub(ptr, i32, i32)\n";
    out_ << "declare ptr  @__tl_str_upper(ptr)\n";
    out_ << "declare ptr  @__tl_str_lower(ptr)\n";
    out_ << "declare ptr  @__tl_str_trim(ptr)\n";
    out_ << "declare i32  @__tl_str_contains(ptr, ptr)\n";
    out_ << "declare i32  @__tl_str_starts_with(ptr, ptr)\n";
    out_ << "declare i32  @__tl_str_ends_with(ptr, ptr)\n";
    out_ << "declare i32  @__tl_str_index_of(ptr, ptr)\n";
    out_ << "declare ptr  @__tl_str_replace(ptr, ptr, ptr)\n";
    out_ << "declare i32  @__tl_str_to_int(ptr)\n";
    out_ << "declare double @__tl_str_to_float(ptr)\n";
    out_ << "declare i32  @__tl_str_char_at(ptr, i32)\n";
    out_ << "declare i32  @__tl_file_exists(ptr)\n";
    out_ << "declare ptr  @__tl_file_read_all(ptr)\n";
    out_ << "declare i32  @__tl_file_write_all(ptr, ptr)\n";
    out_ << "declare i32  @__tl_file_append(ptr, ptr)\n";
    out_ << "declare i32  @__tl_file_delete(ptr)\n";
    out_ << "\n";
}

// ---------------------------------------------------------------------------
// Function emission
// ---------------------------------------------------------------------------

void LLVMGen::emitFunc(const TIR::Func& fn, bool isMain) {
    regTypes_.clear();
    allocSlots_.clear();
    paramRegs_.clear();
    tmpCounter_ = 0;

    std::string key = isMain ? "__global__"
                             : (fn.className.empty() ? fn.name
                                                     : fn.className + "::" + fn.name);
    TIR::Type retTy = isMain ? TIR::Type::i32() : callRetType(key);
    std::string sym  = isMain ? "main" : funcSym(fn.name, fn.className);

    out_ << "define " << llvmType(retTy) << " @" << sym << "(";

    bool hasThis = !fn.className.empty() && !isMain;
    int  argOffset = 0;
    if (hasThis) {
        out_ << "ptr %arg_this";
        argOffset = 1;
        if (!fn.params.empty()) out_ << ", ";
    }
    for (int i = 0; i < (int)fn.params.size(); ++i) {
        if (i > 0) out_ << ", ";
        out_ << llvmType(fn.params[i].first) << " %arg" << (i + argOffset);
    }
    out_ << ") {\n";

    for (auto& blk : fn.blocks)
        emitBlock(blk, fn, isMain, retTy);

    out_ << "}\n\n";
}

// ---------------------------------------------------------------------------
// Block emission
// ---------------------------------------------------------------------------

void LLVMGen::emitBlock(const TIR::Block& blk, const TIR::Func& fn,
                         bool isMain, const TIR::Type& retTy) {
    out_ << blk.label << ":\n";
    for (auto& ins : blk.instrs) emitInstr(ins);
    emitTerm(blk.term, isMain, retTy);
}

// ---------------------------------------------------------------------------
// Instruction emission
// ---------------------------------------------------------------------------

void LLVMGen::emitInstr(const TIR::Instr& ins) {
    using Op = TIR::Op;

    switch (ins.op) {

    // ── Parameter reference: no LLVM instruction; just record the alias ───
    case Op::ParamRef:
        paramRegs_[ins.dest] = ins.ival;
        regTypes_[ins.dest]  = ins.type;
        break;

    // ── alloca ────────────────────────────────────────────────────────────
    case Op::Alloc:
        out_ << "  " << regRef(ins.dest) << " = alloca "
             << llvmType(ins.type) << ", align " << alignOf(ins.type) << "\n";
        allocSlots_.insert(ins.dest);
        regTypes_[ins.dest] = ins.type;
        break;

    // ── load ──────────────────────────────────────────────────────────────
    case Op::Load: {
        const TIR::Val& slot = ins.args[0];
        out_ << "  " << regRef(ins.dest) << " = load "
             << llvmType(ins.type) << ", ptr " << regRef(slot.reg)
             << ", align " << alignOf(ins.type) << "\n";
        regTypes_[ins.dest] = ins.type;
        break;
    }

    // ── store ─────────────────────────────────────────────────────────────
    case Op::Store: {
        const TIR::Val& val  = ins.args[0];
        const TIR::Val& slot = ins.args[1];
        TIR::Type vty = effectiveType(val);
        if (vty.isVoid()) vty = ins.type;
        out_ << "  store " << llvmType(vty) << " " << llvmVal(val)
             << ", ptr " << regRef(slot.reg)
             << ", align " << alignOf(vty) << "\n";
        break;
    }

    // ── arithmetic ────────────────────────────────────────────────────────
    case Op::Add: case Op::Sub: case Op::Mul: case Op::Div: {
        const TIR::Val& l = ins.args[0];
        const TIR::Val& r = ins.args[1];
        TIR::Type ty = effectiveType(l);
        if (ty.isVoid()) ty = ins.type;
        bool fp = ty.isF64();
        const char* llop =
            ins.op == Op::Add ? (fp ? "fadd" : "add")  :
            ins.op == Op::Sub ? (fp ? "fsub" : "sub")  :
            ins.op == Op::Mul ? (fp ? "fmul" : "mul")  :
                                (fp ? "fdiv" : "sdiv");
        out_ << "  " << regRef(ins.dest) << " = " << llop
             << " " << llvmType(ty) << " " << llvmVal(l)
             << ", " << llvmVal(r) << "\n";
        regTypes_[ins.dest] = ty;
        break;
    }

    case Op::Neg: {
        const TIR::Val& a = ins.args[0];
        TIR::Type ty = effectiveType(a);
        if (ty.isVoid()) ty = ins.type;
        if (ty.isF64())
            out_ << "  " << regRef(ins.dest) << " = fneg double " << llvmVal(a) << "\n";
        else
            out_ << "  " << regRef(ins.dest) << " = sub i32 0, " << llvmVal(a) << "\n";
        regTypes_[ins.dest] = ty;
        break;
    }

    // ── comparisons → i1 ──────────────────────────────────────────────────
    case Op::CmpEq: case Op::CmpNe: case Op::CmpLt: case Op::CmpGt: {
        const TIR::Val& l = ins.args[0];
        const TIR::Val& r = ins.args[1];
        TIR::Type ty = effectiveType(l);
        if (ty.isVoid()) ty = ins.type;
        bool fp = ty.isF64();
        const char* llop =
            ins.op == Op::CmpEq ? (fp ? "fcmp oeq" : "icmp eq")  :
            ins.op == Op::CmpNe ? (fp ? "fcmp one" : "icmp ne")  :
            ins.op == Op::CmpLt ? (fp ? "fcmp olt" : "icmp slt") :
                                  (fp ? "fcmp ogt" : "icmp sgt");
        out_ << "  " << regRef(ins.dest) << " = " << llop
             << " " << llvmType(ty) << " " << llvmVal(l)
             << ", " << llvmVal(r) << "\n";
        regTypes_[ins.dest] = TIR::Type::i1();
        break;
    }

    // ── logical ───────────────────────────────────────────────────────────
    case Op::And:
        out_ << "  " << regRef(ins.dest) << " = and i1 "
             << llvmVal(ins.args[0]) << ", " << llvmVal(ins.args[1]) << "\n";
        regTypes_[ins.dest] = TIR::Type::i1();
        break;

    case Op::Or:
        out_ << "  " << regRef(ins.dest) << " = or i1 "
             << llvmVal(ins.args[0]) << ", " << llvmVal(ins.args[1]) << "\n";
        regTypes_[ins.dest] = TIR::Type::i1();
        break;

    case Op::Not:
        out_ << "  " << regRef(ins.dest) << " = xor i1 "
             << llvmVal(ins.args[0]) << ", 1\n";
        regTypes_[ins.dest] = TIR::Type::i1();
        break;

    // ── casts ─────────────────────────────────────────────────────────────
    case Op::CastI32: {
        const TIR::Val& a = ins.args[0];
        TIR::Type from = effectiveType(a);
        if (from.isF64())
            out_ << "  " << regRef(ins.dest) << " = fptosi double " << llvmVal(a) << " to i32\n";
        else if (from.isI1())
            out_ << "  " << regRef(ins.dest) << " = zext i1 " << llvmVal(a) << " to i32\n";
        else
            out_ << "  " << regRef(ins.dest) << " = add i32 0, " << llvmVal(a) << "\n";
        regTypes_[ins.dest] = TIR::Type::i32();
        break;
    }

    case Op::CastF64: {
        const TIR::Val& a = ins.args[0];
        TIR::Type from = effectiveType(a);
        if (from.isI32() || from.isI1() || from.isChar())
            out_ << "  " << regRef(ins.dest) << " = sitofp i32 " << llvmVal(a) << " to double\n";
        else
            out_ << "  " << regRef(ins.dest) << " = fadd double " << llvmVal(a) << ", 0.0\n";
        regTypes_[ins.dest] = TIR::Type::f64();
        break;
    }

    case Op::CastChar: {
        // Truncate to 8-bit character range (low byte), keep as i32.
        out_ << "  " << regRef(ins.dest) << " = and i32 "
             << llvmVal(ins.args[0]) << ", 255\n";
        regTypes_[ins.dest] = TIR::Type::i32();
        break;
    }

    case Op::CastStr: {
        const TIR::Val& a = ins.args[0];
        TIR::Type from = effectiveType(a);
        if (from.isI32() || from.isI1())
            out_ << "  " << regRef(ins.dest) << " = call ptr @__tl_i32_to_str(i32 " << llvmVal(a) << ")\n";
        else if (from.isF64())
            out_ << "  " << regRef(ins.dest) << " = call ptr @__tl_f64_to_str(double " << llvmVal(a) << ")\n";
        else
            out_ << "  " << regRef(ins.dest) << " = add ptr 0, " << llvmVal(a) << "\n";
        regTypes_[ins.dest] = TIR::Type::str();
        break;
    }

    case Op::CastI1: {
        const TIR::Val& a = ins.args[0];
        TIR::Type from = effectiveType(a);
        if (from.isF64())
            out_ << "  " << regRef(ins.dest) << " = fcmp one double " << llvmVal(a) << ", 0.0\n";
        else if (from.isI1())
            out_ << "  " << regRef(ins.dest) << " = add i1 0, " << llvmVal(a) << "\n";
        else
            out_ << "  " << regRef(ins.dest) << " = icmp ne i32 " << llvmVal(a) << ", 0\n";
        regTypes_[ins.dest] = TIR::Type::i1();
        break;
    }

    // ── I/O ──────────────────────────────────────────────────────────────
    case Op::Print: {
        TIR::Val a = ins.args[0];
        TIR::Type ty = effectiveType(a);
        if (ty.isVoid()) ty = ins.type;
        std::string val = llvmVal(a);

        if (ty.isI1()) {
            std::string ext = tmp("pext");
            out_ << "  " << ext << " = zext i1 " << val << " to i32\n";
            out_ << "  call void @__tl_print_i32(i32 " << ext << ")\n";
        } else if (ty.isI32() || ty.isChar()) {
            out_ << "  call void @__tl_print_i32(i32 " << val << ")\n";
        } else if (ty.isF64()) {
            out_ << "  call void @__tl_print_f64(double " << val << ")\n";
        } else if (ty.isStr()) {
            out_ << "  call void @__tl_print_str(ptr " << val << ")\n";
        } else {
            out_ << "  ; print <unsupported type " << (int)ty.base << ">\n";
        }
        break;
    }

    case Op::Input:
        out_ << "  " << regRef(ins.dest) << " = call i32 @__tl_input_i32()\n";
        regTypes_[ins.dest] = TIR::Type::i32();
        break;

    // ── free-function call ─────────────────────────────────────────────────
    case Op::Call: {
        TIR::Type retTy = callRetType(ins.name);
        std::string sym  = funcSym(ins.name, "");
        bool hasRet = !retTy.isVoid() && ins.dest != TIR::NOREG;

        out_ << "  ";
        if (hasRet) { out_ << regRef(ins.dest) << " = "; regTypes_[ins.dest] = retTy; }
        out_ << "call " << llvmType(retTy) << " @" << sym << "(";
        for (int i = 0; i < (int)ins.args.size(); ++i) {
            if (i > 0) out_ << ", ";
            TIR::Type at = effectiveType(ins.args[i]);
            if (at.isVoid()) at = ins.args[i].type;
            out_ << llvmType(at) << " " << llvmVal(ins.args[i]);
        }
        out_ << ")\n";
        break;
    }

    // ── OOP ──────────────────────────────────────────────────────────────
    case Op::PushThis:
        // In a method body, 'this' was passed as the first ptr argument.
        out_ << "  " << regRef(ins.dest) << " = select i1 true, ptr %arg_this, ptr null\n";
        regTypes_[ins.dest] = ins.type;
        break;

    case Op::NewObj: {
        std::string cls = ins.name2.empty() ? ins.name : ins.name2;
        int numFields = 0;
        auto cit = prog_->classes.find(cls);
        if (cit != prog_->classes.end())
            numFields = (int)cit->second.fields.size();

        out_ << "  " << regRef(ins.dest)
             << " = call ptr @__tl_alloc_obj(i32 " << numFields << ")\n";
        regTypes_[ins.dest] = TIR::Type::obj(cls);

        std::string ctorKey = cls + "::init";
        if (prog_->funcs.count(ctorKey)) {
            out_ << "  call void @_TL_" << cls << "__init(ptr " << regRef(ins.dest);
            for (auto& a : ins.args) {
                TIR::Type at = effectiveType(a);
                out_ << ", " << llvmType(at) << " " << llvmVal(a);
            }
            out_ << ")\n";
        }
        break;
    }

    case Op::LoadField: {
        const TIR::Val& obj = ins.args[0];
        int fieldIdx = resolveFieldIndex(obj, ins.name);
        std::string raw = tmp("fld");
        out_ << "  " << raw << " = call i64 @__tl_load_field(ptr "
             << llvmVal(obj) << ", i32 " << fieldIdx << ")\n";
        if (ins.type.isI32() || ins.type.isChar() || ins.type.isI1()) {
            out_ << "  " << regRef(ins.dest) << " = trunc i64 " << raw << " to i32\n";
            regTypes_[ins.dest] = TIR::Type::i32();
        } else if (ins.type.isF64()) {
            out_ << "  " << regRef(ins.dest) << " = bitcast i64 " << raw << " to double\n";
            regTypes_[ins.dest] = TIR::Type::f64();
        } else {
            out_ << "  " << regRef(ins.dest) << " = inttoptr i64 " << raw << " to ptr\n";
            regTypes_[ins.dest] = ins.type;
        }
        break;
    }

    case Op::StoreField: {
        // args[0]=val, args[1]=obj
        const TIR::Val& val = ins.args[0];
        const TIR::Val& obj = ins.args[1];
        int fieldIdx = resolveFieldIndex(obj, ins.name);
        TIR::Type vty = effectiveType(val);
        std::string raw = tmp("sfv");
        if (vty.isI32() || vty.isChar() || vty.isI1())
            out_ << "  " << raw << " = zext i32 " << llvmVal(val) << " to i64\n";
        else if (vty.isF64())
            out_ << "  " << raw << " = bitcast double " << llvmVal(val) << " to i64\n";
        else
            out_ << "  " << raw << " = ptrtoint ptr " << llvmVal(val) << " to i64\n";
        out_ << "  call void @__tl_store_field(ptr " << llvmVal(obj)
             << ", i32 " << fieldIdx << ", i64 " << raw << ")\n";
        break;
    }

    case Op::CallMethod: {
        std::string cls = ins.name2;
        std::string key = cls + "::" + ins.name;
        TIR::Type retTy = callRetType(key);
        bool hasRet = !retTy.isVoid() && ins.dest != TIR::NOREG;
        out_ << "  ";
        if (hasRet) { out_ << regRef(ins.dest) << " = "; regTypes_[ins.dest] = retTy; }
        out_ << "call " << llvmType(retTy) << " @_TL_" << cls << "__" << ins.name
             << "(ptr " << llvmVal(ins.args[0]);
        for (int i = 1; i < (int)ins.args.size(); ++i) {
            TIR::Type at = effectiveType(ins.args[i]);
            out_ << ", " << llvmType(at) << " " << llvmVal(ins.args[i]);
        }
        out_ << ")\n";
        break;
    }

    case Op::CallSuper: {
        std::string cls = ins.name2;
        std::string key = cls + "::" + ins.name;
        TIR::Type retTy = callRetType(key);
        bool hasRet = !retTy.isVoid() && ins.dest != TIR::NOREG;
        out_ << "  ";
        if (hasRet) { out_ << regRef(ins.dest) << " = "; regTypes_[ins.dest] = retTy; }
        out_ << "call " << llvmType(retTy) << " @_TL_" << cls << "__" << ins.name
             << "(ptr %arg_this";
        for (auto& a : ins.args) {
            TIR::Type at = effectiveType(a);
            out_ << ", " << llvmType(at) << " " << llvmVal(a);
        }
        out_ << ")\n";
        break;
    }

    // ── Arrays ────────────────────────────────────────────────────────────
    case Op::NewArray: {
        if (ins.ival >= 0) {
            out_ << "  " << regRef(ins.dest)
                 << " = call ptr @__tl_alloc_arr(i32 " << ins.ival << ")\n";
        } else {
            TIR::Type st = effectiveType(ins.args[0]);
            out_ << "  " << regRef(ins.dest)
                 << " = call ptr @__tl_alloc_arr("
                 << llvmType(st) << " " << llvmVal(ins.args[0]) << ")\n";
        }
        regTypes_[ins.dest] = TIR::Type::arr(ins.name);
        break;
    }

    case Op::LoadArr: {
        const TIR::Val& arr = ins.args[0];
        const TIR::Val& idx = ins.args[1];
        TIR::Type ity = effectiveType(idx);
        std::string raw = tmp("arrl");
        out_ << "  " << raw << " = call i64 @__tl_load_arr(ptr "
             << llvmVal(arr) << ", " << llvmType(ity) << " " << llvmVal(idx) << ")\n";
        if (ins.type.isI32() || ins.type.isChar() || ins.type.isI1()) {
            out_ << "  " << regRef(ins.dest) << " = trunc i64 " << raw << " to i32\n";
            regTypes_[ins.dest] = TIR::Type::i32();
        } else if (ins.type.isF64()) {
            out_ << "  " << regRef(ins.dest) << " = bitcast i64 " << raw << " to double\n";
            regTypes_[ins.dest] = TIR::Type::f64();
        } else {
            out_ << "  " << regRef(ins.dest) << " = inttoptr i64 " << raw << " to ptr\n";
            regTypes_[ins.dest] = ins.type;
        }
        break;
    }

    case Op::StoreArr: {
        // args[0]=val, args[1]=arr, args[2]=idx
        const TIR::Val& val = ins.args[0];
        const TIR::Val& arr = ins.args[1];
        const TIR::Val& idx = ins.args[2];
        TIR::Type vty = effectiveType(val);
        TIR::Type ity = effectiveType(idx);
        std::string raw = tmp("arsv");
        if (vty.isI32() || vty.isChar() || vty.isI1())
            out_ << "  " << raw << " = zext i32 " << llvmVal(val) << " to i64\n";
        else if (vty.isF64())
            out_ << "  " << raw << " = bitcast double " << llvmVal(val) << " to i64\n";
        else
            out_ << "  " << raw << " = ptrtoint ptr " << llvmVal(val) << " to i64\n";
        out_ << "  call void @__tl_store_arr(ptr " << llvmVal(arr)
             << ", " << llvmType(ity) << " " << llvmVal(idx)
             << ", i64 " << raw << ")\n";
        break;
    }

    case Op::Nop:
        break;

    default:
        out_ << "  ; unhandled op " << (int)ins.op << "\n";
        break;
    }
}

// ---------------------------------------------------------------------------
// Terminator emission
// ---------------------------------------------------------------------------

void LLVMGen::emitTerm(const TIR::Term& term, bool isMain,
                        const TIR::Type& retTy) {
    switch (term.kind) {

    case TIR::TermKind::Ret:
        if (isMain)
            out_ << "  ret i32 0\n";
        else if (retTy.isVoid())
            out_ << "  ret void\n";
        else
            out_ << "  unreachable\n";   // dead block after a real ret
        break;

    case TIR::TermKind::RetVal: {
        if (isMain) { out_ << "  ret i32 0\n"; break; }
        TIR::Type ety = effectiveType(term.val);
        if (ety.isVoid()) ety = retTy;
        // If comparison result (i1) but function returns i32, zext first.
        if (ety.isI1() && retTy.isI32()) {
            std::string ext = tmp("rext");
            out_ << "  " << ext << " = zext i1 " << llvmVal(term.val) << " to i32\n";
            out_ << "  ret i32 " << ext << "\n";
        } else {
            out_ << "  ret " << llvmType(ety) << " " << llvmVal(term.val) << "\n";
        }
        break;
    }

    case TIR::TermKind::Br:
        out_ << "  br label %" << term.target << "\n";
        break;

    case TIR::TermKind::BrCond: {
        TIR::Type condTy = effectiveType(term.cond);
        std::string condVal = llvmVal(term.cond);
        if (!condTy.isI1()) {
            std::string t = tmp("cond");
            if (condTy.isF64())
                out_ << "  " << t << " = fcmp one double " << condVal << ", 0.0\n";
            else
                out_ << "  " << t << " = icmp ne i32 " << condVal << ", 0\n";
            condVal = t;
        }
        out_ << "  br i1 " << condVal
             << ", label %" << term.trueTarget
             << ", label %" << term.falseTarget << "\n";
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Field index resolution helper
// ---------------------------------------------------------------------------

int LLVMGen::resolveFieldIndex(const TIR::Val& objVal,
                                const std::string& fieldName) const {
    std::string cls;
    TIR::Type ot = effectiveType(objVal);
    if (ot.isObj()) cls = ot.name;
    if (cls.empty() && objVal.isReg()) {
        auto it = regTypes_.find(objVal.reg);
        if (it != regTypes_.end() && it->second.isObj())
            cls = it->second.name;
    }
    auto cit = prog_->classes.find(cls);
    if (cit != prog_->classes.end()) {
        auto& fields = cit->second.fields;
        for (int i = 0; i < (int)fields.size(); ++i)
            if (fields[i].second == fieldName) return i;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Top-level emit
// ---------------------------------------------------------------------------

std::string LLVMGen::emit(const TIR::Program& prog) {
    prog_       = &prog;
    strCounter_ = 0;
    tmpCounter_ = 0;
    out_.str(""); out_.clear();
    strGlobals_.clear();
    funcRetTypes_.clear();

    for (auto& [k, fn] : prog.funcs) collectStrings(fn);
    collectStrings(prog.globalInit);

    buildRetTypeMap();

    out_ << "; Generated by TinyLang LLVM backend (Phase 4.5)\n";
    out_ << "; Compile: clang <this.ll> runtime/native/tinyrt.c -o program\n";
    out_ << "source_filename = \"tinylang\"\n";
    out_ << "target triple = \"arm64-apple-macosx15.0.0\"\n\n";

    emitStringGlobals();
    emitRuntimeDecls();

    for (auto& [k, fn] : prog.funcs)
        emitFunc(fn, false);

    emitFunc(prog.globalInit, true);

    return out_.str();
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------

std::string emitLLVM(const TIR::Program& prog) {
    LLVMGen gen;
    return gen.emit(prog);
}
