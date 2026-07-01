#include "tirvm.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

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
    auto it = prog_->funcs.find(funcKey);
    if (it == prog_->funcs.end())
        throw std::runtime_error("TIRVM: undefined function: " + funcKey);

    const TIR::Func& fn = it->second;
    TIRFrame frame;
    frame.func      = &fn;
    frame.className = className.empty() ? fn.className : className;
    frame.thisObj   = thisObj;

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
