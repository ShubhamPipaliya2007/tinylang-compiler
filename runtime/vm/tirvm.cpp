#include "tirvm.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>

// ─────────────────────────────────────────────────────────────────────────────
// Utilities
// ─────────────────────────────────────────────────────────────────────────────

std::string TIRVM::newHandle(const std::string& pfx) {
    return "__" + pfx + std::to_string(handleCounter_++);
}

IRValue TIRVM::defaultValue(TIR::Type ty) {
    if (ty.isI32() || ty.isI1()) return IRValue::fromInt(0);
    if (ty.isF64())               return IRValue::fromFloat(0.0);
    if (ty.isChar())              return IRValue::fromChar('\0');
    if (ty.isStr())               return IRValue::fromString("");
    return IRValue::nil();
}

IRValue TIRVM::evalVal(const TIR::Val& v, const TIRFrame& frame) const {
    if (v.isConst()) {
        switch (v.type.base) {
        case TIR::BaseType::I32:  return IRValue::fromInt(v.ival);
        case TIR::BaseType::F64:  return IRValue::fromFloat(v.dval);
        case TIR::BaseType::I1:   return IRValue::fromInt(v.ival);
        case TIR::BaseType::Char: return IRValue::fromChar(v.cval);
        case TIR::BaseType::Str:  return IRValue::fromString(v.sval);
        default:                  return IRValue::nil();
        }
    }
    // Register reference
    auto it = frame.regs.find(v.reg);
    if (it != frame.regs.end()) return it->second;
    throw std::runtime_error("TIRVM: undefined register %" + std::to_string(v.reg));
}

std::string TIRVM::valueToString(const IRValue& v) const {
    switch (v.type) {
    case IRType::INT:        return std::to_string(v.i);
    case IRType::FLOAT: {
        std::string s = std::to_string(v.f);
        s.erase(s.find_last_not_of('0') + 1);
        if (s.back() == '.') s.pop_back();
        return s;
    }
    case IRType::CHAR:       return std::string(1, v.c);
    case IRType::STRING:     return v.s;
    case IRType::OBJ_HANDLE: return v.s;
    case IRType::ARR_HANDLE: return v.s;
    default:                 return "nil";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Arithmetic / comparison  (same semantics as IRVM)
// ─────────────────────────────────────────────────────────────────────────────

IRValue TIRVM::arith(const IRValue& l, const IRValue& r, TIR::Op op) const {
    if (op == TIR::Op::Add &&
        (l.type == IRType::STRING || r.type == IRType::STRING))
        return IRValue::fromString(valueToString(l) + valueToString(r));

    if (l.type == IRType::FLOAT || r.type == IRType::FLOAT) {
        double lv = (l.type==IRType::FLOAT) ? l.f : (l.type==IRType::CHAR)?(double)(int)l.c:(double)l.i;
        double rv = (r.type==IRType::FLOAT) ? r.f : (r.type==IRType::CHAR)?(double)(int)r.c:(double)r.i;
        switch (op) {
        case TIR::Op::Add: return IRValue::fromFloat(lv + rv);
        case TIR::Op::Sub: return IRValue::fromFloat(lv - rv);
        case TIR::Op::Mul: return IRValue::fromFloat(lv * rv);
        case TIR::Op::Div:
            if (rv == 0.0) throw std::runtime_error("TIRVM: division by zero");
            return IRValue::fromFloat(lv / rv);
        default: break;
        }
    }
    int lv = (l.type==IRType::CHAR) ? (int)l.c : l.i;
    int rv = (r.type==IRType::CHAR) ? (int)r.c : r.i;
    switch (op) {
    case TIR::Op::Add: return IRValue::fromInt(lv + rv);
    case TIR::Op::Sub: return IRValue::fromInt(lv - rv);
    case TIR::Op::Mul: return IRValue::fromInt(lv * rv);
    case TIR::Op::Div:
        if (rv == 0) throw std::runtime_error("TIRVM: division by zero");
        return IRValue::fromInt(lv / rv);
    default: break;
    }
    throw std::runtime_error("TIRVM: unsupported arithmetic op");
}

IRValue TIRVM::compare(const IRValue& l, const IRValue& r, TIR::Op op) const {
    if (l.type == IRType::STRING || r.type == IRType::STRING ||
        l.type == IRType::OBJ_HANDLE || r.type == IRType::OBJ_HANDLE) {
        std::string ls = valueToString(l), rs = valueToString(r);
        if (op == TIR::Op::CmpEq) return IRValue::fromInt(ls==rs?1:0);
        if (op == TIR::Op::CmpNe) return IRValue::fromInt(ls!=rs?1:0);
        throw std::runtime_error("TIRVM: < / > not supported for strings");
    }
    if (l.type == IRType::CHAR && r.type == IRType::CHAR) {
        if (op == TIR::Op::CmpEq) return IRValue::fromInt(l.c==r.c?1:0);
        if (op == TIR::Op::CmpNe) return IRValue::fromInt(l.c!=r.c?1:0);
        if (op == TIR::Op::CmpLt) return IRValue::fromInt(l.c< r.c?1:0);
        if (op == TIR::Op::CmpGt) return IRValue::fromInt(l.c> r.c?1:0);
    }
    bool isFloat = (l.type==IRType::FLOAT || r.type==IRType::FLOAT);
    if (isFloat) {
        double lv = (l.type==IRType::FLOAT)?l.f:(double)l.i;
        double rv = (r.type==IRType::FLOAT)?r.f:(double)r.i;
        if (op == TIR::Op::CmpEq) return IRValue::fromInt(lv==rv?1:0);
        if (op == TIR::Op::CmpNe) return IRValue::fromInt(lv!=rv?1:0);
        if (op == TIR::Op::CmpLt) return IRValue::fromInt(lv< rv?1:0);
        if (op == TIR::Op::CmpGt) return IRValue::fromInt(lv> rv?1:0);
    } else {
        if (op == TIR::Op::CmpEq) return IRValue::fromInt(l.i==r.i?1:0);
        if (op == TIR::Op::CmpNe) return IRValue::fromInt(l.i!=r.i?1:0);
        if (op == TIR::Op::CmpLt) return IRValue::fromInt(l.i< r.i?1:0);
        if (op == TIR::Op::CmpGt) return IRValue::fromInt(l.i> r.i?1:0);
    }
    throw std::runtime_error("TIRVM: unsupported comparison op");
}

// ─────────────────────────────────────────────────────────────────────────────
// Object / class helpers
// ─────────────────────────────────────────────────────────────────────────────

void TIRVM::collectAllFields(const std::string& cls,
                              std::vector<std::pair<std::string,std::string>>& out) const {
    auto it = prog_->classes.find(cls);
    if (it == prog_->classes.end()) return;
    if (!it->second.baseClass.empty())
        collectAllFields(it->second.baseClass, out);
    for (auto& field : it->second.fields) {
        const std::string ftyStr  = field.first.asStr();
        const std::string& fname  = field.second;
        auto fi = std::find_if(out.begin(), out.end(),
                               [&fname](const auto& x){ return x.second == fname; });
        if (fi != out.end()) fi->first = ftyStr;
        else                 out.emplace_back(ftyStr, fname);
    }
}

void TIRVM::initObjectFields(const std::string& cls, IRObject& obj) {
    std::vector<std::pair<std::string,std::string>> fields;
    collectAllFields(cls, fields);
    for (auto& [type, name] : fields) {
        if      (type == "int"   || type == "i32" || type == "bool" || type == "i1")
            obj.fields[name] = IRValue::fromInt(0);
        else if (type == "float" || type == "f64")
            obj.fields[name] = IRValue::fromFloat(0.0);
        else if (type == "char")
            obj.fields[name] = IRValue::fromChar('\0');
        else if (type == "string" || type == "str")
            obj.fields[name] = IRValue::fromString("");
        else
            obj.fields[name] = IRValue::nil();
    }
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
// Re-load each field slot in the current frame from the heap so stale copies
// do not overwrite the values the callee just set (mirrors IRVM's re-sync).
void TIRVM::resyncFieldSlots(TIRFrame& frame, const std::string& handle) {
    if (handle.empty() || !objHeap_.count(handle)) return;
    if (!frame.func || frame.func->blocks.empty()) return;
    const auto& obj = objHeap_.at(handle);
    for (auto& ins : frame.func->blocks[0].instrs) {
        if (ins.op != TIR::Op::Alloc || ins.name.empty()) continue;
        auto fit = obj.fields.find(ins.name);
        if (fit != obj.fields.end())
            frame.mem[ins.dest] = fit->second;
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
                                    const std::vector<IRValue>& callArgs) {
    for (auto& ins : block.instrs) {
        switch (ins.op) {

        // ── Alloc: create a mutable slot with a default value ──────────────
        case TIR::Op::Alloc:
            frame.mem[ins.dest] = defaultValue(ins.type);
            break;

        // ── Load from alloc slot ───────────────────────────────────────────
        case TIR::Op::Load: {
            TIR::Reg slot = ins.args[0].reg;
            auto mit = frame.mem.find(slot);
            if (mit == frame.mem.end())
                throw std::runtime_error("TIRVM: load from uninitialised slot %" +
                                         std::to_string(slot));
            frame.regs[ins.dest] = mit->second;
            break;
        }

        // ── Store to alloc slot ────────────────────────────────────────────
        case TIR::Op::Store: {
            IRValue val     = evalVal(ins.args[0], frame);
            TIR::Reg slot   = ins.args[1].reg;
            frame.mem[slot] = val;
            break;
        }

        // ── ParamRef: get the Nth function argument ────────────────────────
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
            IRValue l = evalVal(ins.args[0], frame);
            IRValue r = evalVal(ins.args[1], frame);
            frame.regs[ins.dest] = arith(l, r, ins.op);
            break;
        }
        case TIR::Op::Neg: {
            IRValue v = evalVal(ins.args[0], frame);
            frame.regs[ins.dest] = (v.type==IRType::FLOAT)
                ? IRValue::fromFloat(-v.f) : IRValue::fromInt(-v.i);
            break;
        }

        // ── Comparison ─────────────────────────────────────────────────────
        case TIR::Op::CmpEq: case TIR::Op::CmpNe:
        case TIR::Op::CmpLt: case TIR::Op::CmpGt: {
            IRValue l = evalVal(ins.args[0], frame);
            IRValue r = evalVal(ins.args[1], frame);
            frame.regs[ins.dest] = compare(l, r, ins.op);
            break;
        }

        // ── Logical ────────────────────────────────────────────────────────
        case TIR::Op::And: {
            IRValue l = evalVal(ins.args[0], frame);
            IRValue r = evalVal(ins.args[1], frame);
            frame.regs[ins.dest] = IRValue::fromInt(l.isTruthy()&&r.isTruthy()?1:0);
            break;
        }
        case TIR::Op::Or: {
            IRValue l = evalVal(ins.args[0], frame);
            IRValue r = evalVal(ins.args[1], frame);
            frame.regs[ins.dest] = IRValue::fromInt(l.isTruthy()||r.isTruthy()?1:0);
            break;
        }
        case TIR::Op::Not: {
            IRValue v = evalVal(ins.args[0], frame);
            frame.regs[ins.dest] = IRValue::fromInt(v.isTruthy()?0:1);
            break;
        }

        // ── Casts ──────────────────────────────────────────────────────────
        case TIR::Op::CastI32: {
            IRValue v = evalVal(ins.args[0], frame);
            int iv = (v.type==IRType::FLOAT)   ? (int)v.f
                   : (v.type==IRType::CHAR)    ? (int)v.c
                   : (v.type==IRType::STRING)  ? (v.s.empty()?0:(int)v.s[0])
                   : v.i;
            frame.regs[ins.dest] = IRValue::fromInt(iv);
            break;
        }
        case TIR::Op::CastF64: {
            IRValue v = evalVal(ins.args[0], frame);
            double dv = (v.type==IRType::FLOAT)  ? v.f
                      : (v.type==IRType::CHAR)   ? (double)(int)v.c
                      : (double)v.i;
            frame.regs[ins.dest] = IRValue::fromFloat(dv);
            break;
        }
        case TIR::Op::CastChar: {
            IRValue v = evalVal(ins.args[0], frame);
            char cv = (v.type==IRType::CHAR) ? v.c : (char)v.i;
            frame.regs[ins.dest] = IRValue::fromChar(cv);
            break;
        }
        case TIR::Op::CastI1: {
            IRValue v = evalVal(ins.args[0], frame);
            frame.regs[ins.dest] = IRValue::fromInt(v.isTruthy()?1:0);
            break;
        }
        case TIR::Op::CastStr: {
            IRValue v = evalVal(ins.args[0], frame);
            frame.regs[ins.dest] = IRValue::fromString(valueToString(v));
            break;
        }

        // ── I/O ────────────────────────────────────────────────────────────
        case TIR::Op::Print: {
            IRValue v = evalVal(ins.args[0], frame);
            switch (v.type) {
            case IRType::STRING: std::cout << v.s << "\n"; break;
            case IRType::FLOAT:  std::cout << v.f << "\n"; break;
            case IRType::CHAR:   std::cout << v.c << "\n"; break;
            default:             std::cout << v.i << "\n"; break;
            }
            break;
        }
        case TIR::Op::Input: {
            int val; std::cin >> val;
            frame.regs[ins.dest] = IRValue::fromInt(val);
            break;
        }
        case TIR::Op::ReadFile: {
            std::ifstream f(ins.name);
            if (!f.is_open())
                throw std::runtime_error("TIRVM: cannot open file: " + ins.name);
            int val; f >> val;
            frame.regs[ins.dest] = IRValue::fromInt(val);
            break;
        }

        // ── Free function call ─────────────────────────────────────────────
        case TIR::Op::Call: {
            std::vector<IRValue> args;
            for (auto& a : ins.args) args.push_back(evalVal(a, frame));
            IRValue ret = callFunc(ins.name, args);
            frame.regs[ins.dest] = ret;
            break;
        }

        // ── OOP ────────────────────────────────────────────────────────────
        case TIR::Op::PushThis:
            frame.regs[ins.dest] = IRValue::objHandle(frame.thisHandle);
            break;

        case TIR::Op::NewObj: {
            std::string cls = ins.name;
            std::string handle = newHandle("obj");
            IRObject obj;
            obj.className = cls;
            initObjectFields(cls, obj);
            objHeap_[handle] = obj;

            // Call constructor if args provided
            if (!ins.args.empty()) {
                std::vector<IRValue> args;
                for (auto& a : ins.args) args.push_back(evalVal(a, frame));
                // look for constructor method "init" or class name
                auto* ctor = findMethodCached(cls, "init");
                if (!ctor) ctor = findMethodCached(cls, cls);
                if (ctor) callFunc(ctor->className + "::" + ctor->name,
                                   args, handle, cls);
            }
            frame.regs[ins.dest] = IRValue::objHandle(handle);
            break;
        }

        case TIR::Op::LoadField: {
            IRValue objVal = evalVal(ins.args[0], frame);
            std::string handle = objVal.s;
            auto oit = objHeap_.find(handle);
            if (oit == objHeap_.end())
                throw std::runtime_error("TIRVM: invalid object handle: " + handle);
            auto fit = oit->second.fields.find(ins.name);
            frame.regs[ins.dest] = (fit != oit->second.fields.end())
                ? fit->second : IRValue::nil();
            break;
        }

        case TIR::Op::StoreField: {
            IRValue val    = evalVal(ins.args[0], frame);
            IRValue objVal = evalVal(ins.args[1], frame);
            std::string handle = objVal.s;
            auto oit = objHeap_.find(handle);
            if (oit == objHeap_.end())
                throw std::runtime_error("TIRVM: invalid object handle: " + handle);
            oit->second.fields[ins.name] = val;
            break;
        }

        case TIR::Op::CallMethod: {
            // args[0] = obj handle; args[1..] = method arguments
            IRValue objVal = evalVal(ins.args[0], frame);
            std::string handle = objVal.s;
            auto oit = objHeap_.find(handle);
            if (oit == objHeap_.end())
                throw std::runtime_error("TIRVM: invalid object handle for method call: " + handle);
            std::string cls = oit->second.className;
            const TIR::Func* mfn = findMethodCached(cls, ins.name);
            if (!mfn)
                throw std::runtime_error("TIRVM: method not found: " + cls + "::" + ins.name);

            std::vector<IRValue> margs;
            for (size_t i = 1; i < ins.args.size(); ++i)
                margs.push_back(evalVal(ins.args[i], frame));

            IRValue ret = callFunc(mfn->className + "::" + mfn->name,
                                   margs, handle, cls);
            // Re-sync: if the called method mutated this object's fields
            // and the object is also the current frame's "this", pull updates in.
            if (handle == frame.thisHandle)
                resyncFieldSlots(frame, handle);
            frame.regs[ins.dest] = ret;
            break;
        }

        case TIR::Op::CallSuper: {
            // Find parent class of current frame's class
            std::string parentCls;
            auto ci = prog_->classes.find(frame.className);
            if (ci != prog_->classes.end()) parentCls = ci->second.baseClass;

            if (parentCls.empty())
                throw std::runtime_error("TIRVM: no parent class for super call");
            const TIR::Func* mfn = findMethodCached(parentCls, ins.name);
            if (!mfn)
                throw std::runtime_error("TIRVM: super method not found: " + ins.name);

            std::vector<IRValue> margs;
            for (auto& a : ins.args) margs.push_back(evalVal(a, frame));

            IRValue ret = callFunc(mfn->className + "::" + mfn->name,
                                   margs, frame.thisHandle, parentCls);
            // Re-sync: the super call may have updated shared fields on this object.
            resyncFieldSlots(frame, frame.thisHandle);
            frame.regs[ins.dest] = ret;
            break;
        }

        // ── Arrays ─────────────────────────────────────────────────────────
        case TIR::Op::NewArray: {
            std::string elemType = ins.name2;
            std::string handle   = newHandle("arr");
            IRArray arr;
            arr.elemType = elemType;

            if (ins.ival >= 0) {
                // Literal elements (args = element values)
                for (auto& a : ins.args) arr.elements.push_back(evalVal(a, frame));
            } else {
                // Size from args[0]
                int sz = (int)evalVal(ins.args[0], frame).i;
                arr.elements.resize(sz, defaultValue(TIR::Type::i32()));
            }
            arrHeap_[handle] = std::move(arr);
            frame.regs[ins.dest] = IRValue::arrHandle(handle);
            break;
        }

        case TIR::Op::LoadArr: {
            IRValue arrVal = evalVal(ins.args[0], frame);
            IRValue idxVal = evalVal(ins.args[1], frame);
            std::string handle = arrVal.s;
            auto ait = arrHeap_.find(handle);
            if (ait == arrHeap_.end())
                throw std::runtime_error("TIRVM: invalid array handle: " + handle);
            int idx = idxVal.i;
            if (idx < 0 || idx >= (int)ait->second.elements.size())
                throw std::runtime_error("TIRVM: array index out of bounds: " + std::to_string(idx));
            frame.regs[ins.dest] = ait->second.elements[idx];
            break;
        }

        case TIR::Op::StoreArr: {
            IRValue val    = evalVal(ins.args[0], frame);
            IRValue arrVal = evalVal(ins.args[1], frame);
            IRValue idxVal = evalVal(ins.args[2], frame);
            std::string handle = arrVal.s;
            auto ait = arrHeap_.find(handle);
            if (ait == arrHeap_.end())
                throw std::runtime_error("TIRVM: invalid array handle: " + handle);
            int idx = idxVal.i;
            if (idx < 0 || idx >= (int)ait->second.elements.size())
                throw std::runtime_error("TIRVM: array index out of bounds: " + std::to_string(idx));
            ait->second.elements[idx] = val;
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
        return {ExecResult::Ret, IRValue::nil(), "", false};

    switch (block.term.kind) {
    case TIR::TermKind::Ret:
        return {ExecResult::Ret, IRValue::nil(), "", false};
    case TIR::TermKind::RetVal:
        return {ExecResult::RetVal, evalVal(block.term.val, frame), "", false};
    case TIR::TermKind::Br:
        return {ExecResult::Br, IRValue::nil(), block.term.target, false};
    case TIR::TermKind::BrCond: {
        bool taken = evalVal(block.term.cond, frame).isTruthy();
        return {ExecResult::BrCond, IRValue::nil(),
                taken ? block.term.trueTarget : block.term.falseTarget, taken};
    }
    }
    return {ExecResult::Ret, IRValue::nil(), "", false};
}

// ─────────────────────────────────────────────────────────────────────────────
// Function call
// ─────────────────────────────────────────────────────────────────────────────

IRValue TIRVM::callFunc(const std::string& funcKey,
                         const std::vector<IRValue>& args,
                         const std::string& thisHandle,
                         const std::string& className) {
    auto it = prog_->funcs.find(funcKey);
    if (it == prog_->funcs.end())
        throw std::runtime_error("TIRVM: undefined function: " + funcKey);

    const TIR::Func& fn = it->second;
    TIRFrame frame;
    frame.func       = &fn;
    frame.className  = className.empty() ? fn.className : className;
    frame.thisHandle = thisHandle;

    // Execute blocks following control-flow edges
    std::string currentLabel = "entry";
    while (true) {
        // Find the block
        const TIR::Block* block = nullptr;
        for (auto& b : fn.blocks)
            if (b.label == currentLabel) { block = &b; break; }
        if (!block)
            throw std::runtime_error("TIRVM: block not found: " + currentLabel);

        ExecResult res = execBlock(*block, frame, args);

        switch (res.kind) {
        case ExecResult::Ret:    return IRValue::nil();
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
    // Execute global initialiser (top-level code)
    const TIR::Func& gi = prog.globalInit;
    if (gi.blocks.empty()) return;

    // Register globalInit in funcs map temporarily so callFunc can find it
    // Actually, we execute it directly without callFunc:
    TIRFrame frame;
    frame.func       = &gi;
    frame.className  = "";
    frame.thisHandle = "";

    std::vector<IRValue> noArgs;
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
