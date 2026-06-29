#include "tir.hpp"
#include <iostream>
#include <sstream>

namespace TIR {

// ─── Type::str ────────────────────────────────────────────────────────────────
std::string Type::asStr() const {
    switch (base) {
    case BaseType::Void:   return "void";
    case BaseType::I1:     return "i1";
    case BaseType::I32:    return "i32";
    case BaseType::F64:    return "f64";
    case BaseType::Char:   return "char";
    case BaseType::Str:    return "str";
    case BaseType::ObjRef: return "obj<" + name + ">";
    case BaseType::ArrRef: return "arr<" + name + ">";
    }
    return "?";
}

// ─── Val::str ─────────────────────────────────────────────────────────────────
std::string Val::asStr() const {
    if (isReg()) return "%" + std::to_string(reg);
    switch (type.base) {
    case BaseType::I32:  return std::to_string(ival);
    case BaseType::F64:  { std::ostringstream os; os << dval; return os.str(); }
    case BaseType::I1:   return ival ? "true" : "false";
    case BaseType::Char: return std::string("'") + cval + "'";
    case BaseType::Str:  return "\"" + sval + "\"";
    default:             return "nil";
    }
}

// ─── Opcode name ──────────────────────────────────────────────────────────────
static const char* opStr(Op op) {
    switch (op) {
    case Op::Alloc:      return "alloc";
    case Op::Load:       return "load";
    case Op::Store:      return "store";
    case Op::ParamRef:   return "param";
    case Op::Add:        return "add";
    case Op::Sub:        return "sub";
    case Op::Mul:        return "mul";
    case Op::Div:        return "div";
    case Op::Neg:        return "neg";
    case Op::CmpEq:      return "cmp.eq";
    case Op::CmpNe:      return "cmp.ne";
    case Op::CmpLt:      return "cmp.lt";
    case Op::CmpGt:      return "cmp.gt";
    case Op::And:        return "and";
    case Op::Or:         return "or";
    case Op::Not:        return "not";
    case Op::CastI32:    return "cast.i32";
    case Op::CastF64:    return "cast.f64";
    case Op::CastChar:   return "cast.char";
    case Op::CastI1:     return "cast.i1";
    case Op::CastStr:    return "cast.str";
    case Op::Print:      return "print";
    case Op::Input:      return "input";
    case Op::ReadFile:   return "read.file";
    case Op::Call:       return "call";
    case Op::PushThis:   return "this";
    case Op::NewObj:     return "new.obj";
    case Op::LoadField:  return "load.field";
    case Op::StoreField: return "store.field";
    case Op::CallMethod: return "call.method";
    case Op::CallSuper:  return "call.super";
    case Op::NewArray:   return "new.arr";
    case Op::LoadArr:    return "load.arr";
    case Op::StoreArr:   return "store.arr";
    case Op::Phi:        return "phi";
    case Op::Nop:        return "nop";
    }
    return "?";
}

// ─── Single instruction dump ──────────────────────────────────────────────────
static void printInstr(const Instr& ins) {
    const char* I = "    ";
    std::cout << I;

    if (ins.dest != NOREG)
        std::cout << "%" << ins.dest << " : " << ins.type.asStr() << " = ";

    std::cout << opStr(ins.op);

    switch (ins.op) {
    case Op::Alloc:
        std::cout << " " << ins.type.asStr() << " \"" << ins.name << "\"";
        break;

    case Op::Load:
        std::cout << " " << ins.type.asStr();
        if (!ins.args.empty()) std::cout << " " << ins.args[0].asStr();
        break;

    case Op::Store:
        if (ins.args.size() >= 2)
            std::cout << " " << ins.type.asStr()
                      << " " << ins.args[0].asStr()
                      << " -> " << ins.args[1].asStr();
        break;

    case Op::ParamRef:
        std::cout << " " << ins.ival;
        break;

    case Op::Add: case Op::Sub: case Op::Mul: case Op::Div:
    case Op::CmpEq: case Op::CmpNe: case Op::CmpLt: case Op::CmpGt:
    case Op::And: case Op::Or:
        std::cout << " " << ins.type.asStr();
        if (ins.args.size() >= 2)
            std::cout << " " << ins.args[0].asStr() << ", " << ins.args[1].asStr();
        break;

    case Op::Neg: case Op::Not:
    case Op::CastI32: case Op::CastF64: case Op::CastChar:
    case Op::CastI1: case Op::CastStr:
        std::cout << " " << ins.type.asStr();
        if (!ins.args.empty()) std::cout << " " << ins.args[0].asStr();
        break;

    case Op::Print:
        std::cout << " " << ins.type.asStr();
        if (!ins.args.empty()) std::cout << " " << ins.args[0].asStr();
        break;

    case Op::Input:
    case Op::PushThis:
        break;

    case Op::ReadFile:
        std::cout << " \"" << ins.name << "\"";
        break;

    case Op::Call:
        std::cout << " " << ins.type.asStr() << " " << ins.name << "(";
        for (size_t i = 0; i < ins.args.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << ins.args[i].asStr();
        }
        std::cout << ")";
        break;

    case Op::NewObj:
        std::cout << " " << ins.name << "(";
        for (size_t i = 0; i < ins.args.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << ins.args[i].asStr();
        }
        std::cout << ")";
        break;

    case Op::LoadField:
        std::cout << " " << ins.type.asStr();
        if (!ins.args.empty()) std::cout << " " << ins.args[0].asStr();
        std::cout << " \"" << ins.name << "\"";
        break;

    case Op::StoreField:
        if (ins.args.size() >= 2)
            std::cout << " " << ins.type.asStr()
                      << " " << ins.args[0].asStr()
                      << " -> " << ins.args[1].asStr()
                      << " \"" << ins.name << "\"";
        break;

    case Op::CallMethod:
        std::cout << " " << ins.type.asStr()
                  << " " << ins.name2 << "." << ins.name << "(";
        for (size_t i = 0; i < ins.args.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << ins.args[i].asStr();
        }
        std::cout << ")";
        break;

    case Op::CallSuper:
        std::cout << " " << ins.type.asStr() << " super." << ins.name << "(";
        for (size_t i = 0; i < ins.args.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << ins.args[i].asStr();
        }
        std::cout << ")";
        break;

    case Op::NewArray:
        std::cout << " " << ins.name2 << "[";
        if (ins.ival >= 0)            std::cout << ins.ival;
        else if (!ins.args.empty())   std::cout << ins.args[0].asStr();
        std::cout << "]";
        break;

    case Op::LoadArr:
        std::cout << " " << ins.type.asStr();
        if (ins.args.size() >= 2)
            std::cout << " " << ins.args[0].asStr() << "[" << ins.args[1].asStr() << "]";
        break;

    case Op::StoreArr:
        if (ins.args.size() >= 3)
            std::cout << " " << ins.type.asStr()
                      << " " << ins.args[0].asStr()
                      << " -> " << ins.args[1].asStr()
                      << "[" << ins.args[2].asStr() << "]";
        break;

    case Op::Phi: {
        std::cout << " " << ins.type.asStr() << " [";
        for (size_t i = 0; i < ins.phi.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << ins.phi[i].val.asStr() << " from %" << ins.phi[i].predLabel;
        }
        std::cout << "]";
        break;
    }

    default: break;
    }
    std::cout << "\n";
}

// ─── Terminator dump ──────────────────────────────────────────────────────────
static void printTerm(const Term& t) {
    const char* I = "    ";
    std::cout << I;
    switch (t.kind) {
    case TermKind::Ret:
        std::cout << "ret\n"; break;
    case TermKind::RetVal:
        std::cout << "ret " << t.val.getType().asStr() << " " << t.val.asStr() << "\n"; break;
    case TermKind::Br:
        std::cout << "br %" << t.target << "\n"; break;
    case TermKind::BrCond:
        std::cout << "br.cond " << t.cond.asStr()
                  << " %" << t.trueTarget
                  << " %" << t.falseTarget << "\n"; break;
    }
}

// ─── Function dump ────────────────────────────────────────────────────────────
void dumpFunc(const Func& fn) {
    auto header = fn.className.empty() ? fn.name : fn.className + "::" + fn.name;
    std::cout << "\nfunc " << header << "(";
    for (size_t i = 0; i < fn.params.size(); ++i) {
        if (i) std::cout << ", ";
        std::cout << fn.params[i].first.asStr() << " %" << fn.params[i].second;
    }
    std::cout << ") -> " << fn.retType.asStr() << " {\n";

    for (auto& bb : fn.blocks) {
        std::cout << "  " << bb.label << ":\n";
        for (auto& ins : bb.instrs) printInstr(ins);
        if (bb.sealed) printTerm(bb.term);
        else           std::cout << "    [open]\n";
    }
    std::cout << "}\n";
}

// ─── Program dump ─────────────────────────────────────────────────────────────
void dumpProgram(const Program& prog) {
    std::cout << "=== TinyIR (register-based) ===\n";

    for (auto& [n, cls] : prog.classes) {
        std::cout << "\nclass " << n;
        if (!cls.baseClass.empty()) std::cout << " : " << cls.baseClass;
        std::cout << " {\n";
        for (auto& [t, f] : cls.fields)
            std::cout << "  " << (t.asStr()) << " " << f << ";\n";
        std::cout << "}\n";
    }

    for (auto& [key, fn] : prog.funcs) dumpFunc(fn);

    if (!prog.globalInit.blocks.empty()) {
        std::cout << "\n; ── global init ──\n";
        dumpFunc(prog.globalInit);
    }

    std::cout << "=================================\n";
}

} // namespace TIR
