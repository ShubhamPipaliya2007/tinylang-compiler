#pragma once
#include "ir.hpp"
#include <string>
#include <vector>
#include <unordered_map>

// ===========================================================================
// Runtime value type
// ===========================================================================
enum class IRType { NIL, INT, FLOAT, CHAR, STRING, OBJ_HANDLE, ARR_HANDLE };

struct IRValue {
    IRType      type = IRType::NIL;
    int         i    = 0;
    double      f    = 0.0;
    char        c    = 0;
    std::string s;   // STRING text, or handle ID for OBJ_HANDLE / ARR_HANDLE

    // Factories
    static IRValue nil()                              { return {}; }
    static IRValue fromInt(int v)                     { IRValue r; r.type=IRType::INT;    r.i=v; return r; }
    static IRValue fromFloat(double v)                { IRValue r; r.type=IRType::FLOAT;  r.f=v; return r; }
    static IRValue fromChar(char v)                   { IRValue r; r.type=IRType::CHAR;   r.c=v; return r; }
    static IRValue fromString(const std::string& v)   { IRValue r; r.type=IRType::STRING; r.s=v; return r; }
    static IRValue fromBool(bool v)                   { return fromInt(v ? 1 : 0); }
    static IRValue objHandle(const std::string& h)    { IRValue r; r.type=IRType::OBJ_HANDLE; r.s=h; return r; }
    static IRValue arrHandle(const std::string& h)    { IRValue r; r.type=IRType::ARR_HANDLE; r.s=h; return r; }

    bool isTruthy() const {
        if (type == IRType::FLOAT)  return f != 0.0;
        if (type == IRType::CHAR)   return c != 0;
        if (type == IRType::STRING ||
            type == IRType::OBJ_HANDLE ||
            type == IRType::ARR_HANDLE) return !s.empty();
        return i != 0;
    }
};

// ===========================================================================
// Heap objects / arrays
// ===========================================================================
struct IRObject {
    std::string className;
    std::unordered_map<std::string, IRValue> fields;
};

struct IRArray {
    std::string elemType;
    std::vector<IRValue> elements;
};

// ===========================================================================
// Call frame  (one per active function invocation)
// ===========================================================================
struct VMFrame {
    // Stack of nested scopes; index 0 is the function's outermost scope
    std::vector<std::unordered_map<std::string, IRValue>> scopes;
    std::string className;    // class of the executing method ("" for free functions)
    std::string thisHandle;   // "this" object handle ("" for free functions)

    VMFrame() { scopes.push_back({}); }

    void pushScope() { scopes.push_back({}); }
    void popScope()  { if (scopes.size() > 1) scopes.pop_back(); }

    // Walk scopes from innermost to outermost.
    IRValue get(const std::string& name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto f = it->find(name);
            if (f != it->end()) return f->second;
        }
        throw std::runtime_error("Undefined variable: " + name);
    }

    // Update nearest existing scope, or create in innermost.
    void set(const std::string& name, const IRValue& val) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->count(name)) { (*it)[name] = val; return; }
        }
        scopes.back()[name] = val;
    }

    // Always create/overwrite in innermost scope (for DECLARE).
    void declare(const std::string& name, const IRValue& val) {
        scopes.back()[name] = val;
    }
};

// ===========================================================================
// IR Virtual Machine
// ===========================================================================
class IRVM {
public:
    void run(const IRProgram& prog);

private:
    const IRProgram*                            prog_ = nullptr;
    std::vector<IRValue>                        stack_;
    std::unordered_map<std::string, IRObject>   objHeap_;
    std::unordered_map<std::string, IRArray>    arrHeap_;
    int                                         handleCounter_ = 0;

    std::string newHandle(const std::string& pfx = "h");
    void   push(const IRValue& v) { stack_.push_back(v); }
    IRValue pop();

    // Helpers
    std::string valueToString(const IRValue& v) const;
    IRValue arith  (const IRValue& l, const IRValue& r, IROp op);
    IRValue compare(const IRValue& l, const IRValue& r, IROp op);

    // Class hierarchy utilities
    void collectAllFields(const std::string& cls,
                          std::vector<std::pair<std::string,std::string>>& out) const;
    void initObjectFields(const std::string& cls, IRObject& obj);
    const IRFunction* findMethod(const std::string& cls, const std::string& method) const;

    // Execute a code block inside a given frame; returns the return value.
    IRValue runCode(const std::vector<IRInstr>& code, VMFrame& frame);

    // Call a compiled function (free or method); manages frames and field sync.
    IRValue callFunction(const std::string& funcKey,
                         const std::vector<IRValue>& args,
                         const std::string& thisHandle = "",
                         const std::string& className  = "");
};

// Public entry point
void runIR(const IRProgram& prog);
