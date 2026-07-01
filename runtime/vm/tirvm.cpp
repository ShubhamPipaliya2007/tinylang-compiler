#include "tirvm.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <filesystem>

// ─────────────────────────────────────────────────────────────────────────────
// Mark-and-sweep GC
// ─────────────────────────────────────────────────────────────────────────────

void TIRVM::runGC() {
    // Mark phase: walk all roots reachable from every active call frame.
    for (auto* frame : callStack_) {
        if (frame->thisObj)
            heap_.markObject(frame->thisObj);
        for (auto& [reg, val] : frame->regs)
            heap_.markValue(val);
        for (auto& [reg, val] : frame->mem)
            heap_.markValue(val);
    }
    // Sweep phase: free unmarked objects, clear marks on survivors.
    heap_.sweep();
}

// ─────────────────────────────────────────────────────────────────────────────
// Default value for a given TIR type
// ─────────────────────────────────────────────────────────────────────────────

TLValue TIRVM::defaultValue(TIR::Type ty) {
    if (ty.isI32() || ty.isI1()) return TLValue::fromInt(0);
    if (ty.isF64())               return TLValue::fromFloat(0.0);
    if (ty.isChar())              return TLValue::fromChar('\0');
    if (ty.isStr())               return TLValue::fromStr("");
    return TLValue::nil();
}

// ─────────────────────────────────────────────────────────────────────────────
// Evaluate a Val operand → TLValue
// ─────────────────────────────────────────────────────────────────────────────

TLValue TIRVM::evalVal(const TIR::Val& v, const TIRFrame& frame) const {
    if (v.isConst()) {
        switch (v.type.base) {
        case TIR::BaseType::I32:  return TLValue::fromInt(v.ival);
        case TIR::BaseType::F64:  return TLValue::fromFloat(v.dval);
        case TIR::BaseType::I1:   return TLValue::fromInt(v.ival);
        case TIR::BaseType::Char: return TLValue::fromChar(v.cval);
        case TIR::BaseType::Str:  return TLValue::fromStr(v.sval);
        default:                  return TLValue::nil();
        }
    }
    auto it = frame.regs.find(v.reg);
    if (it != frame.regs.end()) return it->second;
    throw std::runtime_error("TIRVM: undefined register %" + std::to_string(v.reg));
}

// ─────────────────────────────────────────────────────────────────────────────
// Value → string (for print and string concatenation)
// ─────────────────────────────────────────────────────────────────────────────

std::string TIRVM::valueToString(const TLValue& v) const {
    switch (v.tag) {
    case TLValue::Tag::Bool:
    case TLValue::Tag::I32:  return std::to_string(v.p.i);
    case TLValue::Tag::F64: {
        std::string s = std::to_string(v.p.d);
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
        return s;
    }
    case TLValue::Tag::Char: return std::string(1, v.p.c);
    case TLValue::Tag::Str:  return v.sval;
    case TLValue::Tag::Obj:  return v.p.obj ? "[" + v.p.obj->className + "]" : "null";
    case TLValue::Tag::Arr:  return "[array]";
    default:                 return "nil";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Arithmetic
// ─────────────────────────────────────────────────────────────────────────────

TLValue TIRVM::arith(const TLValue& l, const TLValue& r, TIR::Op op) const {
    // String concatenation via +
    if (op == TIR::Op::Add &&
        (l.tag == TLValue::Tag::Str || r.tag == TLValue::Tag::Str))
        return TLValue::fromStr(valueToString(l) + valueToString(r));

    // Float promotion
    bool isFloat = (l.tag == TLValue::Tag::F64 || r.tag == TLValue::Tag::F64);
    if (isFloat) {
        double lv = (l.tag==TLValue::Tag::F64)  ? l.p.d
                  : (l.tag==TLValue::Tag::Char) ? (double)(unsigned char)l.p.c
                  : (double)l.p.i;
        double rv = (r.tag==TLValue::Tag::F64)  ? r.p.d
                  : (r.tag==TLValue::Tag::Char) ? (double)(unsigned char)r.p.c
                  : (double)r.p.i;
        switch (op) {
        case TIR::Op::Add: return TLValue::fromFloat(lv + rv);
        case TIR::Op::Sub: return TLValue::fromFloat(lv - rv);
        case TIR::Op::Mul: return TLValue::fromFloat(lv * rv);
        case TIR::Op::Div:
            if (rv == 0.0) throw std::runtime_error("TIRVM: division by zero");
            return TLValue::fromFloat(lv / rv);
        default: break;
        }
    }

    int lv = (l.tag==TLValue::Tag::Char) ? (int)(unsigned char)l.p.c : l.p.i;
    int rv = (r.tag==TLValue::Tag::Char) ? (int)(unsigned char)r.p.c : r.p.i;
    switch (op) {
    case TIR::Op::Add: return TLValue::fromInt(lv + rv);
    case TIR::Op::Sub: return TLValue::fromInt(lv - rv);
    case TIR::Op::Mul: return TLValue::fromInt(lv * rv);
    case TIR::Op::Div:
        if (rv == 0) throw std::runtime_error("TIRVM: division by zero");
        return TLValue::fromInt(lv / rv);
    default: break;
    }
    throw std::runtime_error("TIRVM: unsupported arithmetic op");
}

// ─────────────────────────────────────────────────────────────────────────────
// Comparison
// ─────────────────────────────────────────────────────────────────────────────

TLValue TIRVM::compare(const TLValue& l, const TLValue& r, TIR::Op op) const {
    // String comparison by value
    if (l.tag == TLValue::Tag::Str || r.tag == TLValue::Tag::Str) {
        std::string ls = valueToString(l), rs = valueToString(r);
        if (op == TIR::Op::CmpEq) return TLValue::fromInt(ls==rs ? 1 : 0);
        if (op == TIR::Op::CmpNe) return TLValue::fromInt(ls!=rs ? 1 : 0);
        throw std::runtime_error("TIRVM: < / > not supported for strings");
    }
    // Object comparison by pointer identity
    if (l.tag == TLValue::Tag::Obj || r.tag == TLValue::Tag::Obj) {
        bool eq = (l.p.obj == r.p.obj);
        if (op == TIR::Op::CmpEq) return TLValue::fromInt(eq  ? 1 : 0);
        if (op == TIR::Op::CmpNe) return TLValue::fromInt(!eq ? 1 : 0);
        throw std::runtime_error("TIRVM: < / > not supported for objects");
    }
    if (l.tag == TLValue::Tag::Char && r.tag == TLValue::Tag::Char) {
        if (op == TIR::Op::CmpEq) return TLValue::fromInt(l.p.c==r.p.c ? 1 : 0);
        if (op == TIR::Op::CmpNe) return TLValue::fromInt(l.p.c!=r.p.c ? 1 : 0);
        if (op == TIR::Op::CmpLt) return TLValue::fromInt(l.p.c< r.p.c ? 1 : 0);
        if (op == TIR::Op::CmpGt) return TLValue::fromInt(l.p.c> r.p.c ? 1 : 0);
    }
    bool isFloat = (l.tag==TLValue::Tag::F64 || r.tag==TLValue::Tag::F64);
    if (isFloat) {
        double lv = (l.tag==TLValue::Tag::F64) ? l.p.d : (double)l.p.i;
        double rv = (r.tag==TLValue::Tag::F64) ? r.p.d : (double)r.p.i;
        if (op == TIR::Op::CmpEq) return TLValue::fromInt(lv==rv ? 1 : 0);
        if (op == TIR::Op::CmpNe) return TLValue::fromInt(lv!=rv ? 1 : 0);
        if (op == TIR::Op::CmpLt) return TLValue::fromInt(lv< rv ? 1 : 0);
        if (op == TIR::Op::CmpGt) return TLValue::fromInt(lv> rv ? 1 : 0);
    } else {
        if (op == TIR::Op::CmpEq) return TLValue::fromInt(l.p.i==r.p.i ? 1 : 0);
        if (op == TIR::Op::CmpNe) return TLValue::fromInt(l.p.i!=r.p.i ? 1 : 0);
        if (op == TIR::Op::CmpLt) return TLValue::fromInt(l.p.i< r.p.i ? 1 : 0);
        if (op == TIR::Op::CmpGt) return TLValue::fromInt(l.p.i> r.p.i ? 1 : 0);
    }
    throw std::runtime_error("TIRVM: unsupported comparison op");
}

// ─────────────────────────────────────────────────────────────────────────────
// Object / class helpers
// ─────────────────────────────────────────────────────────────────────────────

void TIRVM::collectAllFields(const std::string& cls,
                              std::vector<std::pair<TIR::Type,std::string>>& out) const {
    auto it = prog_->classes.find(cls);
    if (it == prog_->classes.end()) return;
    if (!it->second.baseClass.empty())
        collectAllFields(it->second.baseClass, out);
    for (auto& field : it->second.fields) {
        const TIR::Type&   fty   = field.first;
        const std::string& fname = field.second;
        auto fi = std::find_if(out.begin(), out.end(),
                               [&fname](const auto& x){ return x.second == fname; });
        if (fi != out.end()) fi->first = fty;
        else                 out.emplace_back(fty, fname);
    }
}

void TIRVM::initObjectFields(const std::string& cls, TLObject* obj) {
    std::vector<std::pair<TIR::Type,std::string>> allFields;
    collectAllFields(cls, allFields);
    obj->fieldDefs = std::move(allFields);
    obj->fields.resize(obj->fieldDefs.size());
    for (int i = 0; i < (int)obj->fieldDefs.size(); ++i)
        obj->fields[i] = defaultValue(obj->fieldDefs[i].first);
}

const TIR::Func* TIRVM::findMethod(const std::string& cls,
                                    const std::string& method) const {
    std::string cur = cls;
    while (!cur.empty()) {
        std::string key = cur + "::" + method;
        auto it = prog_->funcs.find(key);
        if (it != prog_->funcs.end()) return &it->second;
        auto ci = prog_->classes.find(cur);
        if (ci == prog_->classes.end()) break;
        cur = ci->second.baseClass;
    }
    return nullptr;
}

// After a super-call or same-object method-call the heap may have changed.
// Re-read each named field alloc-slot in the current frame from the object's
// field vector, so stale copies don't overwrite values the callee just set.
void TIRVM::resyncFieldSlots(TIRFrame& frame, TLObject* obj) {
    if (!obj || !frame.func || frame.func->blocks.empty()) return;
    for (auto& ins : frame.func->blocks[0].instrs) {
        if (ins.op != TIR::Op::Alloc || ins.name.empty()) continue;
        int idx = obj->fieldIndex(ins.name);
        if (idx >= 0)
            frame.mem[ins.dest] = obj->fields[idx];
    }
}

const TIR::Func* TIRVM::findMethodCached(const std::string& cls,
                                          const std::string& method) {
    std::string key = cls + "::" + method;
    auto it = methodCache_.find(key);
    if (it != methodCache_.end()) return it->second;
    const TIR::Func* fn = findMethod(cls, method);
    methodCache_[key] = fn;
    return fn;
}

// ─────────────────────────────────────────────────────────────────────────────
// Execute a single basic block
// ─────────────────────────────────────────────────────────────────────────────

TIRVM::ExecResult TIRVM::execBlock(const TIR::Block& block,
                                    TIRFrame& frame,
                                    const std::vector<TLValue>& callArgs) {
    for (auto& ins : block.instrs) {
        switch (ins.op) {

        // ── Memory model ───────────────────────────────────────────────────
        case TIR::Op::Alloc:
            frame.mem[ins.dest] = defaultValue(ins.type);
            break;

        case TIR::Op::Load: {
            TIR::Reg slot = ins.args[0].reg;
            auto mit = frame.mem.find(slot);
            if (mit == frame.mem.end())
                throw std::runtime_error("TIRVM: load from uninitialised slot %" +
                                         std::to_string(slot));
            frame.regs[ins.dest] = mit->second;
            break;
        }

        case TIR::Op::Store: {
            TLValue val   = evalVal(ins.args[0], frame);
            TIR::Reg slot = ins.args[1].reg;
            frame.mem[slot] = val;
            break;
        }

        case TIR::Op::ParamRef: {
            int idx = ins.ival;
            if (idx < 0 || idx >= (int)callArgs.size())
                throw std::runtime_error("TIRVM: param " + std::to_string(idx) +
                                         " out of range (nargs=" +
                                         std::to_string(callArgs.size()) + ")");
            frame.regs[ins.dest] = callArgs[idx];
            break;
        }

        // ── Arithmetic ─────────────────────────────────────────────────────
        case TIR::Op::Add: case TIR::Op::Sub:
        case TIR::Op::Mul: case TIR::Op::Div: {
            TLValue l = evalVal(ins.args[0], frame);
            TLValue r = evalVal(ins.args[1], frame);
            frame.regs[ins.dest] = arith(l, r, ins.op);
            break;
        }
        case TIR::Op::Neg: {
            TLValue v = evalVal(ins.args[0], frame);
            frame.regs[ins.dest] = v.isFloat()
                ? TLValue::fromFloat(-v.p.d) : TLValue::fromInt(-v.p.i);
            break;
        }

        // ── Comparison ─────────────────────────────────────────────────────
        case TIR::Op::CmpEq: case TIR::Op::CmpNe:
        case TIR::Op::CmpLt: case TIR::Op::CmpGt: {
            TLValue l = evalVal(ins.args[0], frame);
            TLValue r = evalVal(ins.args[1], frame);
            frame.regs[ins.dest] = compare(l, r, ins.op);
            break;
        }

        // ── Logical ────────────────────────────────────────────────────────
        case TIR::Op::And: {
            TLValue l = evalVal(ins.args[0], frame);
            TLValue r = evalVal(ins.args[1], frame);
            frame.regs[ins.dest] = TLValue::fromInt(l.isTruthy() && r.isTruthy() ? 1 : 0);
            break;
        }
        case TIR::Op::Or: {
            TLValue l = evalVal(ins.args[0], frame);
            TLValue r = evalVal(ins.args[1], frame);
            frame.regs[ins.dest] = TLValue::fromInt(l.isTruthy() || r.isTruthy() ? 1 : 0);
            break;
        }
        case TIR::Op::Not: {
            TLValue v = evalVal(ins.args[0], frame);
            frame.regs[ins.dest] = TLValue::fromInt(v.isTruthy() ? 0 : 1);
            break;
        }

        // ── Casts ──────────────────────────────────────────────────────────
        case TIR::Op::CastI32: {
            TLValue v = evalVal(ins.args[0], frame);
            int iv = (v.tag==TLValue::Tag::F64)  ? (int)v.p.d
                   : (v.tag==TLValue::Tag::Char)  ? (int)(unsigned char)v.p.c
                   : (v.tag==TLValue::Tag::Str)   ? (v.sval.empty() ? 0 : (int)v.sval[0])
                   : v.p.i;
            frame.regs[ins.dest] = TLValue::fromInt(iv);
            break;
        }
        case TIR::Op::CastF64: {
            TLValue v = evalVal(ins.args[0], frame);
            double dv = (v.tag==TLValue::Tag::F64)  ? v.p.d
                      : (v.tag==TLValue::Tag::Char)  ? (double)(unsigned char)v.p.c
                      : (double)v.p.i;
            frame.regs[ins.dest] = TLValue::fromFloat(dv);
            break;
        }
        case TIR::Op::CastChar: {
            TLValue v = evalVal(ins.args[0], frame);
            char cv = (v.tag==TLValue::Tag::Char) ? v.p.c : (char)v.p.i;
            frame.regs[ins.dest] = TLValue::fromChar(cv);
            break;
        }
        case TIR::Op::CastI1: {
            TLValue v = evalVal(ins.args[0], frame);
            frame.regs[ins.dest] = TLValue::fromInt(v.isTruthy() ? 1 : 0);
            break;
        }
        case TIR::Op::CastStr: {
            TLValue v = evalVal(ins.args[0], frame);
            frame.regs[ins.dest] = TLValue::fromStr(valueToString(v));
            break;
        }

        // ── I/O ────────────────────────────────────────────────────────────
        case TIR::Op::Print: {
            TLValue v = evalVal(ins.args[0], frame);
            switch (v.tag) {
            case TLValue::Tag::Str:  std::cout << v.sval    << "\n"; break;
            case TLValue::Tag::F64:  std::cout << v.p.d     << "\n"; break;
            case TLValue::Tag::Char: std::cout << v.p.c     << "\n"; break;
            default:                 std::cout << v.p.i     << "\n"; break;
            }
            break;
        }
        case TIR::Op::Input: {
            int val; std::cin >> val;
            frame.regs[ins.dest] = TLValue::fromInt(val);
            break;
        }
        case TIR::Op::ReadFile: {
            std::ifstream f(ins.name);
            if (!f.is_open())
                throw std::runtime_error("TIRVM: cannot open file: " + ins.name);
            int val; f >> val;
            frame.regs[ins.dest] = TLValue::fromInt(val);
            break;
        }

        // ── Free function call ─────────────────────────────────────────────
        case TIR::Op::Call: {
            std::vector<TLValue> args;
            for (auto& a : ins.args) args.push_back(evalVal(a, frame));
            frame.regs[ins.dest] = callFunc(ins.name, args);
            break;
        }

        // ── OOP ────────────────────────────────────────────────────────────
        case TIR::Op::PushThis:
            frame.regs[ins.dest] = TLValue::fromObj(frame.thisObj);
            break;

        case TIR::Op::NewObj: {
            std::string cls  = ins.name;
            TLObject*   obj  = heap_.allocObject(cls);
            initObjectFields(cls, obj);

            if (!ins.args.empty()) {
                std::vector<TLValue> args;
                for (auto& a : ins.args) args.push_back(evalVal(a, frame));
                auto* ctor = findMethodCached(cls, "init");
                if (!ctor) ctor = findMethodCached(cls, cls);
                if (ctor) callFunc(ctor->className + "::" + ctor->name, args, obj, cls);
            }
            frame.regs[ins.dest] = TLValue::fromObj(obj);
            if (heap_.shouldCollect()) runGC();
            break;
        }

        case TIR::Op::LoadField: {
            TLValue objVal = evalVal(ins.args[0], frame);
            if (!objVal.isObj() || !objVal.p.obj)
                throw std::runtime_error("TIRVM: LoadField on null/non-object");
            frame.regs[ins.dest] = objVal.p.obj->getField(ins.name);
            break;
        }

        case TIR::Op::StoreField: {
            TLValue val    = evalVal(ins.args[0], frame);
            TLValue objVal = evalVal(ins.args[1], frame);
            if (!objVal.isObj() || !objVal.p.obj)
                throw std::runtime_error("TIRVM: StoreField on null/non-object");
            objVal.p.obj->setField(ins.name, val);
            break;
        }

        case TIR::Op::CallMethod: {
            TLValue objVal = evalVal(ins.args[0], frame);
            if (!objVal.isObj() || !objVal.p.obj)
                throw std::runtime_error("TIRVM: CallMethod on null/non-object");
            TLObject* obj = objVal.p.obj;
            const TIR::Func* mfn = findMethodCached(obj->className, ins.name);
            if (!mfn)
                throw std::runtime_error("TIRVM: method not found: " +
                                         obj->className + "::" + ins.name);

            std::vector<TLValue> margs;
            for (size_t i = 1; i < ins.args.size(); ++i)
                margs.push_back(evalVal(ins.args[i], frame));

            TLValue ret = callFunc(mfn->className + "::" + mfn->name,
                                   margs, obj, obj->className);
            if (obj == frame.thisObj)
                resyncFieldSlots(frame, obj);
            frame.regs[ins.dest] = ret;
            break;
        }

        case TIR::Op::CallSuper: {
            std::string parentCls;
            auto ci = prog_->classes.find(frame.className);
            if (ci != prog_->classes.end()) parentCls = ci->second.baseClass;
            if (parentCls.empty())
                throw std::runtime_error("TIRVM: no parent class for super call");

            const TIR::Func* mfn = findMethodCached(parentCls, ins.name);
            if (!mfn)
                throw std::runtime_error("TIRVM: super method not found: " + ins.name);

            std::vector<TLValue> margs;
            for (auto& a : ins.args) margs.push_back(evalVal(a, frame));

            TLValue ret = callFunc(mfn->className + "::" + mfn->name,
                                   margs, frame.thisObj, parentCls);
            resyncFieldSlots(frame, frame.thisObj);
            frame.regs[ins.dest] = ret;
            break;
        }

        // ── Arrays ─────────────────────────────────────────────────────────
        case TIR::Op::NewArray: {
            TLArray* arr = heap_.allocArray(ins.name2);
            if (ins.ival >= 0) {
                for (auto& a : ins.args) arr->elements.push_back(evalVal(a, frame));
            } else {
                int sz = evalVal(ins.args[0], frame).p.i;
                arr->elements.resize(sz, defaultValue(TIR::Type::i32()));
            }
            frame.regs[ins.dest] = TLValue::fromArr(arr);
            if (heap_.shouldCollect()) runGC();
            break;
        }

        case TIR::Op::LoadArr: {
            TLValue arrVal = evalVal(ins.args[0], frame);
            TLValue idxVal = evalVal(ins.args[1], frame);
            if (!arrVal.isArr() || !arrVal.p.arr)
                throw std::runtime_error("TIRVM: LoadArr on null/non-array");
            int idx = idxVal.p.i;
            TLArray* arr = arrVal.p.arr;
            if (idx < 0 || idx >= (int)arr->elements.size())
                throw std::runtime_error("TIRVM: array index out of bounds: " +
                                         std::to_string(idx));
            frame.regs[ins.dest] = arr->elements[idx];
            break;
        }

        case TIR::Op::StoreArr: {
            TLValue val    = evalVal(ins.args[0], frame);
            TLValue arrVal = evalVal(ins.args[1], frame);
            TLValue idxVal = evalVal(ins.args[2], frame);
            if (!arrVal.isArr() || !arrVal.p.arr)
                throw std::runtime_error("TIRVM: StoreArr on null/non-array");
            int idx = idxVal.p.i;
            TLArray* arr = arrVal.p.arr;
            if (idx < 0 || idx >= (int)arr->elements.size())
                throw std::runtime_error("TIRVM: array index out of bounds: " +
                                         std::to_string(idx));
            arr->elements[idx] = val;
            break;
        }

        case TIR::Op::Nop:
            break;

        default:
            throw std::runtime_error("TIRVM: unimplemented opcode " +
                                     std::to_string((int)ins.op));
        }
    }

    // ── Process terminator ─────────────────────────────────────────────────
    if (!block.sealed)
        return {ExecResult::Ret, TLValue::nil(), "", false};

    switch (block.term.kind) {
    case TIR::TermKind::Ret:
        return {ExecResult::Ret, TLValue::nil(), "", false};
    case TIR::TermKind::RetVal:
        return {ExecResult::RetVal, evalVal(block.term.val, frame), "", false};
    case TIR::TermKind::Br:
        return {ExecResult::Br, TLValue::nil(), block.term.target, false};
    case TIR::TermKind::BrCond: {
        bool taken = evalVal(block.term.cond, frame).isTruthy();
        return {ExecResult::BrCond, TLValue::nil(),
                taken ? block.term.trueTarget : block.term.falseTarget, taken};
    }
    }
    return {ExecResult::Ret, TLValue::nil(), "", false};
}

// ─────────────────────────────────────────────────────────────────────────────
// Function call — executes blocks following control-flow edges
// ─────────────────────────────────────────────────────────────────────────────

TLValue TIRVM::callFunc(const std::string& funcKey,
                         const std::vector<TLValue>& args,
                         TLObject* thisObj,
                         const std::string& className) {
    // Dispatch native built-ins (names starting with "__tl_").
    if (funcKey.size() >= 5 && funcKey.compare(0, 5, "__tl_") == 0)
        return callNative(funcKey, args);

    auto it = prog_->funcs.find(funcKey);
    if (it == prog_->funcs.end())
        throw std::runtime_error("TIRVM: undefined function: " + funcKey);

    const TIR::Func& fn = it->second;
    TIRFrame frame;
    frame.func      = &fn;
    frame.className = className.empty() ? fn.className : className;
    frame.thisObj   = thisObj;

    // Register this frame as a GC root for the duration of the call.
    FrameGuard guard(callStack_, &frame);

    std::string currentLabel = "entry";
    while (true) {
        const TIR::Block* block = nullptr;
        for (auto& b : fn.blocks)
            if (b.label == currentLabel) { block = &b; break; }
        if (!block)
            throw std::runtime_error("TIRVM: block not found: " + currentLabel);

        ExecResult res = execBlock(*block, frame, args);
        switch (res.kind) {
        case ExecResult::Ret:    return TLValue::nil();
        case ExecResult::RetVal: return res.val;
        case ExecResult::Br:
        case ExecResult::BrCond:
            currentLabel = res.target;
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Public entry point
// ─────────────────────────────────────────────────────────────────────────────

void TIRVM::run(const TIR::Program& prog) {
    prog_ = &prog;
    const TIR::Func& gi = prog.globalInit;
    if (gi.blocks.empty()) return;

    TIRFrame frame;
    frame.func      = &gi;
    frame.className = "";
    frame.thisObj   = nullptr;

    // Register globalInit frame as a GC root.
    FrameGuard guard(callStack_, &frame);

    std::vector<TLValue> noArgs;
    std::string currentLabel = "entry";
    while (true) {
        const TIR::Block* block = nullptr;
        for (auto& b : gi.blocks)
            if (b.label == currentLabel) { block = &b; break; }
        if (!block) break;

        ExecResult res = execBlock(*block, frame, noArgs);
        switch (res.kind) {
        case ExecResult::Ret:
        case ExecResult::RetVal:
            return;
        case ExecResult::Br:
        case ExecResult::BrCond:
            currentLabel = res.target;
            break;
        }
    }
}

void runTIR(const TIR::Program& prog) {
    TIRVM vm;
    vm.run(prog);
}

// ─────────────────────────────────────────────────────────────────────────────
// Native built-in function dispatch (names beginning with "__tl_")
// ─────────────────────────────────────────────────────────────────────────────

TLValue TIRVM::callNative(const std::string& name, const std::vector<TLValue>& args) {
    auto str0 = [&]() -> const std::string& {
        static const std::string empty;
        return args.empty() ? empty : args[0].sval;
    };
    auto str1 = [&]() -> const std::string& {
        static const std::string empty;
        return args.size() < 2 ? empty : args[1].sval;
    };
    auto i32_0 = [&]() { return args.empty() ? 0 : args[0].p.i; };
    auto i32_1 = [&]() { return args.size() < 2 ? 0 : args[1].p.i; };
    auto i32_2 = [&]() { return args.size() < 3 ? 0 : args[2].p.i; };

    // ── Print (already handled by Op::Print, but support as function too) ──
    if (name == "__tl_print_i32")  { std::cout << i32_0()    << "\n"; return TLValue::nil(); }
    if (name == "__tl_print_f64")  { std::cout << args[0].p.d << "\n"; return TLValue::nil(); }
    if (name == "__tl_print_str")  { std::cout << str0()     << "\n"; return TLValue::nil(); }
    if (name == "__tl_print_bool") { std::cout << (i32_0() ? "true" : "false") << "\n"; return TLValue::nil(); }
    if (name == "__tl_print_char") { std::cout << (char)i32_0() << "\n"; return TLValue::nil(); }

    // ── Conversion ────────────────────────────────────────────────────────
    if (name == "__tl_i32_to_str")  return TLValue::fromStr(std::to_string(i32_0()));
    if (name == "__tl_f64_to_str") {
        std::ostringstream ss; ss << args[0].p.d; return TLValue::fromStr(ss.str());
    }
    if (name == "__tl_bool_to_str") return TLValue::fromStr(i32_0() ? "true" : "false");
    if (name == "__tl_str_concat")  return TLValue::fromStr(str0() + str1());
    if (name == "__tl_str_eq")      return TLValue::fromInt(str0() == str1() ? 1 : 0);

    // ── String ────────────────────────────────────────────────────────────
    if (name == "__tl_str_len")
        return TLValue::fromInt((int)str0().size());

    if (name == "__tl_str_sub") {
        const std::string& s = str0();
        int start = i32_1(), len = i32_2();
        if (start < 0) start = 0;
        if (start >= (int)s.size()) return TLValue::fromStr("");
        return TLValue::fromStr(s.substr(start, std::max(0, len)));
    }

    if (name == "__tl_str_upper") {
        std::string r = str0();
        for (char& c : r) c = (char)std::toupper((unsigned char)c);
        return TLValue::fromStr(r);
    }
    if (name == "__tl_str_lower") {
        std::string r = str0();
        for (char& c : r) c = (char)std::tolower((unsigned char)c);
        return TLValue::fromStr(r);
    }
    if (name == "__tl_str_trim") {
        const std::string& s = str0();
        size_t l = s.find_first_not_of(" \t\n\r\f\v");
        if (l == std::string::npos) return TLValue::fromStr("");
        size_t r = s.find_last_not_of(" \t\n\r\f\v");
        return TLValue::fromStr(s.substr(l, r - l + 1));
    }
    if (name == "__tl_str_contains")
        return TLValue::fromInt(str0().find(str1()) != std::string::npos ? 1 : 0);

    if (name == "__tl_str_starts_with") {
        const std::string& s = str0(), &p = str1();
        return TLValue::fromInt(s.size() >= p.size() && s.compare(0, p.size(), p) == 0 ? 1 : 0);
    }
    if (name == "__tl_str_ends_with") {
        const std::string& s = str0(), &p = str1();
        return TLValue::fromInt(s.size() >= p.size() &&
                                s.compare(s.size() - p.size(), p.size(), p) == 0 ? 1 : 0);
    }
    if (name == "__tl_str_index_of") {
        auto pos = str0().find(str1());
        return TLValue::fromInt(pos == std::string::npos ? -1 : (int)pos);
    }
    if (name == "__tl_str_replace") {
        std::string s = str0();
        const std::string& from = str1();
        const std::string& to   = args.size() < 3 ? "" : args[2].sval;
        if (from.empty()) return TLValue::fromStr(s);
        std::string out;
        size_t pos = 0;
        while (true) {
            size_t f = s.find(from, pos);
            if (f == std::string::npos) { out += s.substr(pos); break; }
            out += s.substr(pos, f - pos) + to;
            pos = f + from.size();
        }
        return TLValue::fromStr(out);
    }
    if (name == "__tl_str_to_int") {
        try { return TLValue::fromInt(std::stoi(str0())); }
        catch (...) { return TLValue::fromInt(0); }
    }
    if (name == "__tl_str_to_float") {
        try { return TLValue::fromFloat(std::stod(str0())); }
        catch (...) { return TLValue::fromFloat(0.0); }
    }
    if (name == "__tl_str_len") // duplicate guard
        return TLValue::fromInt((int)str0().size());
    if (name == "__tl_str_char_at") {
        const std::string& s = str0(); int i = i32_1();
        if (i < 0 || i >= (int)s.size()) return TLValue::fromInt(0);
        return TLValue::fromInt((unsigned char)s[i]);
    }

    // ── Array helpers (used by Vec/Map stdlib classes) ────────────────────
    // Allocate a new array of given capacity pre-filled with default values.
    if (name == "__tl_alloc_arr") {
        int cap = i32_0();
        TLArray* arr = heap_.allocArray("any");
        arr->elements.resize(cap > 0 ? cap : 0, TLValue::nil());
        if (heap_.shouldCollect()) runGC();
        return TLValue::fromArr(arr);
    }
    if (name == "__tl_arr_len") {
        if (args.empty() || !args[0].isArr()) return TLValue::fromInt(0);
        return TLValue::fromInt((int)args[0].p.arr->elements.size());
    }
    if (name == "__tl_load_arr") {
        if (args.size() < 2 || !args[0].isArr()) return TLValue::nil();
        TLArray* arr = args[0].p.arr;
        int idx = i32_1();
        if (idx < 0 || idx >= (int)arr->elements.size()) return TLValue::nil();
        return arr->elements[idx];
    }
    if (name == "__tl_store_arr") {
        // args: (arr, idx, val)
        if (args.size() < 3 || !args[0].isArr()) return TLValue::nil();
        TLArray* arr = args[0].p.arr;
        int idx = i32_1();
        if (idx >= 0 && idx < (int)arr->elements.size())
            arr->elements[idx] = args[2];
        return TLValue::nil();
    }
    // Resize an array: copies existing elements, pads with nil.
    if (name == "__tl_arr_resize") {
        if (args.empty() || !args[0].isArr()) return TLValue::nil();
        TLArray* arr = args[0].p.arr;
        int newCap = i32_1();
        arr->elements.resize(newCap > 0 ? newCap : 0, TLValue::nil());
        return TLValue::fromArr(arr);
    }

    // ── File ─────────────────────────────────────────────────────────────
    if (name == "__tl_file_exists") {
        std::ifstream f(str0());
        return TLValue::fromInt(f.good() ? 1 : 0);
    }
    if (name == "__tl_file_read_all") {
        std::ifstream f(str0());
        if (!f.is_open()) return TLValue::fromStr("");
        std::ostringstream ss; ss << f.rdbuf();
        return TLValue::fromStr(ss.str());
    }
    if (name == "__tl_file_write_all") {
        std::ofstream f(str0());
        if (!f.is_open()) return TLValue::fromInt(0);
        f << str1();
        return TLValue::fromInt(f.good() ? 1 : 0);
    }
    if (name == "__tl_file_append") {
        std::ofstream f(str0(), std::ios::app);
        if (!f.is_open()) return TLValue::fromInt(0);
        f << str1();
        return TLValue::fromInt(f.good() ? 1 : 0);
    }
    if (name == "__tl_file_delete") {
        return TLValue::fromInt(std::remove(str0().c_str()) == 0 ? 1 : 0);
    }

    // ── Directory operations ───────────────────────────────────────────────
    if (name == "__tl_dir_exists") {
        std::error_code ec;
        return TLValue::fromInt(
            std::filesystem::is_directory(str0(), ec) ? 1 : 0);
    }
    if (name == "__tl_dir_create") {
        std::error_code ec;
        return TLValue::fromInt(
            std::filesystem::create_directories(str0(), ec) ? 1 : 0);
    }
    if (name == "__tl_dir_delete") {
        std::error_code ec;
        std::filesystem::remove_all(str0(), ec);
        return TLValue::fromInt(ec ? 0 : 1);
    }
    if (name == "__tl_dir_list") {
        // Returns a TLArray of string entries (filenames only, no path).
        auto* arr = heap_.allocArray("str");
        std::error_code ec;
        std::filesystem::path dirPath(str0());
        if (std::filesystem::is_directory(dirPath, ec)) {
            for (const auto& entry :
                 std::filesystem::directory_iterator(dirPath, ec)) {
                if (entry.path().filename().string()[0] == '.') continue;
                arr->elements.push_back(
                    TLValue::fromStr(entry.path().filename().string()));
            }
        }
        return TLValue::fromArr(arr);
    }
    if (name == "__tl_dir_list_count") {
        std::error_code ec;
        int count = 0;
        for (const auto& entry :
             std::filesystem::directory_iterator(str0(), ec))
            if (entry.path().filename().string()[0] != '.') ++count;
        return TLValue::fromInt(ec ? 0 : count);
    }
    if (name == "__tl_dir_list_entry") {
        std::error_code ec;
        int idx = (args.size() > 1) ? args[1].p.i : 0;
        int i = 0;
        for (const auto& entry :
             std::filesystem::directory_iterator(str0(), ec)) {
            if (entry.path().filename().string()[0] == '.') continue;
            if (i++ == idx)
                return TLValue::fromStr(entry.path().filename().string());
        }
        return TLValue::fromStr("");
    }

    throw std::runtime_error("TIRVM: unknown native function: " + name);
}
