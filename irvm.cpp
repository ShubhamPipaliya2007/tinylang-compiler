#include "irvm.hpp"
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cmath>

// ===========================================================================
// Utility
// ===========================================================================

std::string IRVM::newHandle(const std::string& pfx) {
    return "__" + pfx + std::to_string(handleCounter_++);
}

IRValue IRVM::pop() {
    if (stack_.empty()) throw std::runtime_error("IRVM: operand stack underflow");
    auto v = stack_.back();
    stack_.pop_back();
    return v;
}

std::string IRVM::valueToString(const IRValue& v) const {
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

// ===========================================================================
// Arithmetic + comparison
// ===========================================================================

IRValue IRVM::arith(const IRValue& l, const IRValue& r, IROp op) {
    // String concatenation for ADD
    if (op == IROp::ADD &&
        (l.type == IRType::STRING || r.type == IRType::STRING)) {
        return IRValue::fromString(valueToString(l) + valueToString(r));
    }
    // Float promotion
    if (l.type == IRType::FLOAT || r.type == IRType::FLOAT) {
        double lv = (l.type == IRType::FLOAT) ? l.f
                  : (l.type == IRType::CHAR)  ? (double)(int)l.c : (double)l.i;
        double rv = (r.type == IRType::FLOAT) ? r.f
                  : (r.type == IRType::CHAR)  ? (double)(int)r.c : (double)r.i;
        switch (op) {
            case IROp::ADD: return IRValue::fromFloat(lv + rv);
            case IROp::SUB: return IRValue::fromFloat(lv - rv);
            case IROp::MUL: return IRValue::fromFloat(lv * rv);
            case IROp::DIV:
                if (rv == 0.0) throw std::runtime_error("Division by zero");
                return IRValue::fromFloat(lv / rv);
            default: break;
        }
    }
    // Integer (or char-as-int)
    int lv = (l.type == IRType::CHAR) ? (int)l.c : l.i;
    int rv = (r.type == IRType::CHAR) ? (int)r.c : r.i;
    switch (op) {
        case IROp::ADD: return IRValue::fromInt(lv + rv);
        case IROp::SUB: return IRValue::fromInt(lv - rv);
        case IROp::MUL: return IRValue::fromInt(lv * rv);
        case IROp::DIV:
            if (rv == 0) throw std::runtime_error("Division by zero");
            return IRValue::fromInt(lv / rv);
        default: break;
    }
    throw std::runtime_error("IRVM: unsupported arithmetic op");
}

IRValue IRVM::compare(const IRValue& l, const IRValue& r, IROp op) {
    // String / handle comparison
    if (l.type == IRType::STRING || r.type == IRType::STRING ||
        l.type == IRType::OBJ_HANDLE || r.type == IRType::OBJ_HANDLE) {
        std::string ls = valueToString(l), rs = valueToString(r);
        if (op == IROp::CMP_EQ)  return IRValue::fromInt(ls == rs ? 1 : 0);
        if (op == IROp::CMP_NEQ) return IRValue::fromInt(ls != rs ? 1 : 0);
        throw std::runtime_error("IRVM: <, > not supported for strings");
    }
    // Char comparison
    if (l.type == IRType::CHAR && r.type == IRType::CHAR) {
        if (op == IROp::CMP_EQ)  return IRValue::fromInt(l.c == r.c ? 1 : 0);
        if (op == IROp::CMP_NEQ) return IRValue::fromInt(l.c != r.c ? 1 : 0);
        if (op == IROp::CMP_LT)  return IRValue::fromInt(l.c  < r.c ? 1 : 0);
        if (op == IROp::CMP_GT)  return IRValue::fromInt(l.c  > r.c ? 1 : 0);
    }
    // Numeric (int or float)
    bool isFloat = (l.type == IRType::FLOAT || r.type == IRType::FLOAT);
    if (isFloat) {
        double lv = (l.type == IRType::FLOAT) ? l.f : (double)l.i;
        double rv = (r.type == IRType::FLOAT) ? r.f : (double)r.i;
        if (op == IROp::CMP_EQ)  return IRValue::fromInt(lv == rv ? 1 : 0);
        if (op == IROp::CMP_NEQ) return IRValue::fromInt(lv != rv ? 1 : 0);
        if (op == IROp::CMP_LT)  return IRValue::fromInt(lv  < rv ? 1 : 0);
        if (op == IROp::CMP_GT)  return IRValue::fromInt(lv  > rv ? 1 : 0);
    } else {
        int lv = l.i, rv = r.i;
        if (op == IROp::CMP_EQ)  return IRValue::fromInt(lv == rv ? 1 : 0);
        if (op == IROp::CMP_NEQ) return IRValue::fromInt(lv != rv ? 1 : 0);
        if (op == IROp::CMP_LT)  return IRValue::fromInt(lv  < rv ? 1 : 0);
        if (op == IROp::CMP_GT)  return IRValue::fromInt(lv  > rv ? 1 : 0);
    }
    throw std::runtime_error("IRVM: unsupported comparison op");
}

// ===========================================================================
// Class / object helpers
// ===========================================================================

void IRVM::collectAllFields(const std::string& cls,
                             std::vector<std::pair<std::string,std::string>>& out) const {
    auto it = prog_->classes.find(cls);
    if (it == prog_->classes.end()) return;
    if (!it->second.baseClass.empty())
        collectAllFields(it->second.baseClass, out);
    for (auto& f : it->second.fields) {
        auto fi = std::find_if(out.begin(), out.end(),
                               [&](const auto& x){ return x.second == f.second; });
        if (fi != out.end()) *fi = f; else out.push_back(f);
    }
}

void IRVM::initObjectFields(const std::string& cls, IRObject& obj) {
    std::vector<std::pair<std::string,std::string>> fields;
    collectAllFields(cls, fields);
    for (auto& [type, name] : fields) {
        if      (type == "int"  || type == "bool") obj.fields[name] = IRValue::fromInt(0);
        else if (type == "float")                  obj.fields[name] = IRValue::fromFloat(0.0);
        else if (type == "char")                   obj.fields[name] = IRValue::fromChar('\0');
        else if (type == "string")                 obj.fields[name] = IRValue::fromString("");
        else                                       obj.fields[name] = IRValue::nil();
    }
}

// Walk the inheritance chain to find the nearest defining class for a method.
const IRFunction* IRVM::findMethod(const std::string& cls, const std::string& method) const {
    std::string cur = cls;
    while (!cur.empty()) {
        std::string key = cur + "::" + method;
        auto it = prog_->functions.find(key);
        if (it != prog_->functions.end()) return &it->second;
        auto ci = prog_->classes.find(cur);
        if (ci == prog_->classes.end()) break;
        cur = ci->second.baseClass;
    }
    return nullptr;
}

// ===========================================================================
// Function call  (free functions and class methods)
// ===========================================================================

IRValue IRVM::callFunction(const std::string& funcKey,
                            const std::vector<IRValue>& args,
                            const std::string& thisHandle,
                            const std::string& className) {
    auto it = prog_->functions.find(funcKey);
    if (it == prog_->functions.end())
        throw std::runtime_error("IRVM: undefined function: " + funcKey);
    const IRFunction& fn = it->second;

    if (args.size() != fn.params.size())
        throw std::runtime_error("IRVM: argument count mismatch calling " + funcKey +
                                 " (expected " + std::to_string(fn.params.size()) +
                                 ", got " + std::to_string(args.size()) + ")");

    VMFrame frame;
    frame.className  = className.empty() ? fn.className : className;
    frame.thisHandle = thisHandle;

    // "Fields-as-locals": copy all object fields into scope[0] so that
    // bare LOAD/STORE inside the method body transparently accesses object state.
    if (!thisHandle.empty() && objHeap_.count(thisHandle)) {
        for (auto& [fname, fval] : objHeap_.at(thisHandle).fields)
            frame.scopes[0][fname] = fval;
    }

    // Bind parameters (may shadow field names with the same name)
    for (size_t i = 0; i < args.size(); ++i)
        frame.scopes[0][fn.params[i].second] = args[i];

    // Execute
    IRValue result = runCode(fn.code, frame);

    // Sync updated fields back to the object heap
    if (!thisHandle.empty() && objHeap_.count(thisHandle)) {
        for (auto& [fname, fval] : objHeap_.at(thisHandle).fields) {
            if (frame.scopes[0].count(fname))
                fval = frame.scopes[0].at(fname);
        }
    }

    return result;
}

// ===========================================================================
// Main execution loop
// ===========================================================================

// Pre-scan a code block to build label → next-instruction index.
static std::unordered_map<std::string, size_t>
buildLabelMap(const std::vector<IRInstr>& code) {
    std::unordered_map<std::string, size_t> m;
    for (size_t i = 0; i < code.size(); ++i)
        if (code[i].op == IROp::LABEL)
            m[code[i].sval] = i + 1;
    return m;
}

IRValue IRVM::runCode(const std::vector<IRInstr>& code, VMFrame& frame) {
    auto labels = buildLabelMap(code);
    size_t ip   = 0;

    while (ip < code.size()) {
        const auto& ins = code[ip];
        bool jumped = false;

        switch (ins.op) {

        case IROp::NOP:
        case IROp::LABEL:
            break;

        // ---- Push constants ----
        case IROp::PUSH_INT:   push(IRValue::fromInt(ins.ival));    break;
        case IROp::PUSH_FLOAT: push(IRValue::fromFloat(ins.dval));  break;
        case IROp::PUSH_STR:   push(IRValue::fromString(ins.sval)); break;
        case IROp::PUSH_CHAR:  push(IRValue::fromChar(ins.cval));   break;
        case IROp::PUSH_BOOL:  push(IRValue::fromInt(ins.ival));    break;

        // ---- Variable access ----
        case IROp::LOAD:
            push(frame.get(ins.sval));
            break;

        case IROp::DECLARE:
            frame.declare(ins.sval, pop());
            break;

        case IROp::STORE:
            frame.set(ins.sval, pop());
            break;

        case IROp::POP:
            pop();
            break;

        // ---- Scope management ----
        case IROp::ENTER_SCOPE: frame.pushScope(); break;
        case IROp::EXIT_SCOPE:  frame.popScope();  break;

        // ---- Arithmetic ----
        case IROp::ADD: { auto r=pop(), l=pop(); push(arith(l,r,IROp::ADD)); break; }
        case IROp::SUB: { auto r=pop(), l=pop(); push(arith(l,r,IROp::SUB)); break; }
        case IROp::MUL: { auto r=pop(), l=pop(); push(arith(l,r,IROp::MUL)); break; }
        case IROp::DIV: { auto r=pop(), l=pop(); push(arith(l,r,IROp::DIV)); break; }
        case IROp::NEG: {
            auto v = pop();
            push(v.type == IRType::FLOAT ? IRValue::fromFloat(-v.f) : IRValue::fromInt(-v.i));
            break;
        }

        // ---- Comparison ----
        case IROp::CMP_EQ:
        case IROp::CMP_NEQ:
        case IROp::CMP_LT:
        case IROp::CMP_GT: {
            auto r=pop(), l=pop();
            push(compare(l, r, ins.op));
            break;
        }

        // ---- Logical ----
        case IROp::AND: { auto r=pop(), l=pop(); push(IRValue::fromInt(l.isTruthy()&&r.isTruthy()?1:0)); break; }
        case IROp::OR:  { auto r=pop(), l=pop(); push(IRValue::fromInt(l.isTruthy()||r.isTruthy()?1:0)); break; }
        case IROp::NOT: { auto v=pop(); push(IRValue::fromInt(v.isTruthy()?0:1)); break; }

        // ---- Control flow ----
        case IROp::JUMP:
            if (!labels.count(ins.sval))
                throw std::runtime_error("IRVM: undefined label: " + ins.sval);
            ip = labels.at(ins.sval);
            jumped = true;
            break;

        case IROp::JUMP_FALSE: {
            auto v = pop();
            if (!v.isTruthy()) {
                if (!labels.count(ins.sval))
                    throw std::runtime_error("IRVM: undefined label: " + ins.sval);
                ip = labels.at(ins.sval);
                jumped = true;
            }
            break;
        }

        // ---- Return ----
        case IROp::RETURN:     return IRValue::nil();
        case IROp::RETURN_VAL: return pop();

        // ---- I/O ----
        case IROp::PRINT: {
            auto v = pop();
            switch (v.type) {
                case IRType::STRING: std::cout << v.s   << "\n"; break;
                case IRType::FLOAT:  std::cout << v.f   << "\n"; break;
                case IRType::CHAR:   std::cout << v.c   << "\n"; break;
                default:             std::cout << v.i   << "\n"; break;
            }
            break;
        }
        case IROp::INPUT: {
            int val; std::cin >> val;
            push(IRValue::fromInt(val));
            break;
        }
        case IROp::READ_FILE: {
            std::ifstream file(ins.sval);
            if (!file.is_open())
                throw std::runtime_error("IRVM: cannot open file: " + ins.sval);
            int val; file >> val;
            push(IRValue::fromInt(val));
            break;
        }

        // ---- Casts ----
        case IROp::CAST_INT: {
            auto v = pop();
            if (v.type == IRType::FLOAT)  { push(IRValue::fromInt((int)v.f)); break; }
            if (v.type == IRType::CHAR)   { push(IRValue::fromInt((int)v.c)); break; }
            if (v.type == IRType::STRING) {
                try { push(IRValue::fromInt(std::stoi(v.s))); }
                catch (...) { push(IRValue::fromInt(0)); }
                break;
            }
            push(IRValue::fromInt(v.i));
            break;
        }
        case IROp::CAST_FLOAT: {
            auto v = pop();
            if (v.type == IRType::INT)  { push(IRValue::fromFloat((double)v.i)); break; }
            if (v.type == IRType::CHAR) { push(IRValue::fromFloat((double)(int)v.c)); break; }
            push(IRValue::fromFloat(v.f));
            break;
        }
        case IROp::CAST_CHAR: {
            auto v = pop();
            push(IRValue::fromChar(v.type == IRType::INT ? (char)v.i : v.c));
            break;
        }
        case IROp::CAST_BOOL: {
            auto v = pop();
            push(IRValue::fromInt(v.isTruthy() ? 1 : 0));
            break;
        }
        case IROp::CAST_STR: {
            auto v = pop();
            push(IRValue::fromString(valueToString(v)));
            break;
        }

        // ---- Free function call ----
        case IROp::CALL: {
            int argc = ins.ival;
            std::vector<IRValue> args(argc);
            for (int k = argc - 1; k >= 0; --k) args[k] = pop();
            push(callFunction(ins.sval, args));
            break;
        }

        // ---- Object ----
        case IROp::PUSH_THIS:
            push(IRValue::objHandle(frame.thisHandle));
            break;

        case IROp::NEW_OBJ: {
            int argc = ins.ival;
            std::vector<IRValue> args(argc);
            for (int k = argc - 1; k >= 0; --k) args[k] = pop();

            std::string handle = newHandle("obj");
            IRObject obj;
            obj.className = ins.sval;
            initObjectFields(ins.sval, obj);
            objHeap_[handle] = std::move(obj);

            // Call constructor if args provided
            if (argc > 0) {
                std::string initKey = ins.sval + "::init";
                if (prog_->functions.count(initKey))
                    callFunction(initKey, args, handle, ins.sval);
            }
            push(IRValue::objHandle(handle));
            break;
        }

        case IROp::LOAD_FIELD: {
            auto objVal = pop();
            // May be an OBJ_HANDLE or even a STRING if the old array-proxy hack is used
            std::string handle = objVal.s;
            if (!objHeap_.count(handle))
                throw std::runtime_error("IRVM: unknown object handle '" + handle + "' for LOAD_FIELD " + ins.sval);
            auto& obj = objHeap_.at(handle);
            if (!obj.fields.count(ins.sval))
                throw std::runtime_error("IRVM: field '" + ins.sval + "' not found in object of class " + obj.className);
            push(obj.fields.at(ins.sval));
            break;
        }

        case IROp::STORE_FIELD: {
            // Stack (top→bottom): value, obj_handle
            auto val    = pop();
            auto objVal = pop();
            std::string handle = objVal.s;
            if (!objHeap_.count(handle))
                throw std::runtime_error("IRVM: unknown object handle '" + handle + "' for STORE_FIELD " + ins.sval);
            objHeap_.at(handle).fields[ins.sval] = val;
            // Reflect change into current frame if this is the current "this" object
            if (handle == frame.thisHandle && frame.scopes[0].count(ins.sval))
                frame.scopes[0][ins.sval] = val;
            break;
        }

        case IROp::CALL_METHOD: {
            int argc = ins.ival;
            std::vector<IRValue> args(argc);
            for (int k = argc - 1; k >= 0; --k) args[k] = pop();
            auto objVal = pop(); // obj handle (bottom of args-and-handle region)
            std::string handle = objVal.s;
            if (!objHeap_.count(handle))
                throw std::runtime_error("IRVM: unknown object handle '" + handle + "' for CALL_METHOD " + ins.sval);
            std::string cls = objHeap_.at(handle).className;
            const IRFunction* fn = findMethod(cls, ins.sval);
            if (!fn)
                throw std::runtime_error("IRVM: method '" + ins.sval + "' not found in class " + cls);
            std::string funcKey = fn->className + "::" + fn->name;
            auto result = callFunction(funcKey, args, handle, cls);
            // After the call the method may have altered the object's fields;
            // if the object is also the current "this", keep frame.scopes[0] in sync
            if (handle == frame.thisHandle && objHeap_.count(handle)) {
                for (auto& [fname, fval] : objHeap_.at(handle).fields)
                    if (frame.scopes[0].count(fname)) frame.scopes[0][fname] = fval;
            }
            push(result);
            break;
        }

        case IROp::CALL_SUPER: {
            int argc = ins.ival;
            std::vector<IRValue> args(argc);
            for (int k = argc - 1; k >= 0; --k) args[k] = pop();
            // Resolve base class
            auto ci = prog_->classes.find(frame.className);
            if (ci == prog_->classes.end())
                throw std::runtime_error("IRVM: CALL_SUPER: current class '" + frame.className + "' not found");
            std::string baseClass = ci->second.baseClass;
            if (baseClass.empty())
                throw std::runtime_error("IRVM: CALL_SUPER: class '" + frame.className + "' has no base class");
            const IRFunction* fn = findMethod(baseClass, ins.sval);
            if (!fn)
                throw std::runtime_error("IRVM: super method '" + ins.sval + "' not found in base " + baseClass);
            std::string funcKey = fn->className + "::" + fn->name;
            auto result = callFunction(funcKey, args, frame.thisHandle, baseClass);
            // Re-sync: the super call may have updated the shared object
            if (!frame.thisHandle.empty() && objHeap_.count(frame.thisHandle)) {
                for (auto& [fname, fval] : objHeap_.at(frame.thisHandle).fields)
                    if (frame.scopes[0].count(fname)) frame.scopes[0][fname] = fval;
            }
            push(result);
            break;
        }

        // ---- Arrays ----
        case IROp::NEW_ARRAY: {
            std::string elemType = ins.sval;
            int litCount = ins.ival;
            std::string handle = newHandle("arr");
            IRArray arr;
            arr.elemType = elemType;

            if (litCount == -1) {
                // Size is on the stack
                int size = pop().i;
                IRValue def;
                if      (elemType == "float")  def = IRValue::fromFloat(0.0);
                else if (elemType == "char")   def = IRValue::fromChar('\0');
                else if (elemType == "string") def = IRValue::fromString("");
                else                           def = IRValue::fromInt(0);

                // Object arrays: create individual object instances
                if (prog_->classes.count(elemType)) {
                    for (int k = 0; k < size; ++k) {
                        std::string oHandle = newHandle("obj");
                        IRObject obj;
                        obj.className = elemType;
                        initObjectFields(elemType, obj);
                        objHeap_[oHandle] = std::move(obj);
                        arr.elements.push_back(IRValue::objHandle(oHandle));
                    }
                } else {
                    arr.elements.assign(size, def);
                }
            } else {
                // Literal: pop litCount values (were pushed first-to-last → reverse)
                arr.elements.resize(litCount);
                for (int k = litCount - 1; k >= 0; --k) arr.elements[k] = pop();
            }
            arrHeap_[handle] = std::move(arr);
            push(IRValue::arrHandle(handle));
            break;
        }

        case IROp::ARRAY_LOAD: {
            auto idxVal = pop();
            auto arrVal = pop();
            auto& arr   = arrHeap_.at(arrVal.s);
            int idx = idxVal.i;
            if (idx < 0 || idx >= (int)arr.elements.size())
                throw std::runtime_error("IRVM: array index out of bounds: " + std::to_string(idx));
            push(arr.elements[idx]);
            break;
        }

        case IROp::ARRAY_STORE: {
            auto val    = pop();
            auto idxVal = pop();
            auto arrVal = pop();
            auto& arr   = arrHeap_.at(arrVal.s);
            int idx = idxVal.i;
            if (idx < 0 || idx >= (int)arr.elements.size())
                throw std::runtime_error("IRVM: array index out of bounds: " + std::to_string(idx));
            arr.elements[idx] = val;
            break;
        }

        default:
            throw std::runtime_error("IRVM: unimplemented opcode");
        } // switch

        if (!jumped) ++ip;
    } // while

    return IRValue::nil();
}

// ===========================================================================
// Public entry point
// ===========================================================================

void IRVM::run(const IRProgram& prog) {
    prog_ = &prog;
    VMFrame mainFrame;
    runCode(prog.main, mainFrame);
}

void runIR(const IRProgram& prog) {
    IRVM vm;
    vm.run(prog);
}
