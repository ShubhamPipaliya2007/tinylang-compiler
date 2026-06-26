#pragma once
#include <string>
#include <vector>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Opcodes for the TinyLang IR (stack-based, three-address style labels)
// ---------------------------------------------------------------------------
enum class IROp {
    // Push constants onto the operand stack
    PUSH_INT,        // ival = integer value
    PUSH_FLOAT,      // dval = float value
    PUSH_STR,        // sval = string value
    PUSH_CHAR,       // cval = char value
    PUSH_BOOL,       // ival = 0 or 1

    // Variable access
    LOAD,            // sval = name  →  push value
    DECLARE,         // sval = name  ←  pop value, create in current (innermost) scope
    STORE,           // sval = name  ←  pop value, update nearest existing scope

    // Stack
    POP,             // discard top of stack

    // Arithmetic
    ADD, SUB, MUL, DIV,
    NEG,             // unary minus

    // Comparison  (result = 0 or 1 as INT)
    CMP_EQ, CMP_NEQ, CMP_LT, CMP_GT,

    // Logical
    AND, OR, NOT,

    // Control flow
    JUMP,            // sval = label name
    JUMP_FALSE,      // pop; if 0/false → jump to sval
    LABEL,           // sval = label name  (no-op at runtime, marks position)

    // Functions
    CALL,            // sval = func name, ival = argc
    RETURN,          // return void (pushes NIL so callers are stack-neutral)
    RETURN_VAL,      // return stack top

    // I/O
    PRINT,           // pop and print to stdout
    INPUT,           // push int read from stdin
    READ_FILE,       // sval = filename; push int read from file

    // Explicit type casts  (convert stack top in-place)
    CAST_INT, CAST_FLOAT, CAST_CHAR, CAST_BOOL, CAST_STR,

    // Object operations
    PUSH_THIS,       // push current "this" object handle
    NEW_OBJ,         // sval = className, ival = argc;
                     //   pop argc args → create object → call init if argc>0 → push handle
    LOAD_FIELD,      // sval = fieldName;  pop obj handle → push field value
    STORE_FIELD,     // sval = fieldName;  pop value, pop obj handle → store field
    CALL_METHOD,     // sval = methodName, ival = argc;
                     //   stack (bottom→top): obj_handle, arg0 … argN-1
    CALL_SUPER,      // sval = methodName, ival = argc;  uses current frame's "this"

    // Array operations
    NEW_ARRAY,       // sval = elemType, ival = literal element count
                     //   ival == -1 means pop size from stack, create empty array
    ARRAY_LOAD,      // pop index, pop array handle → push element
    ARRAY_STORE,     // pop value, pop index, pop array handle → store element

    // Lexical scope management within a frame
    ENTER_SCOPE,
    EXIT_SCOPE,

    NOP,
};

// ---------------------------------------------------------------------------
// A single IR instruction
// ---------------------------------------------------------------------------
struct IRInstr {
    IROp        op;
    std::string sval;        // string operand: name, label, type…
    int         ival = 0;    // integer operand or literal value
    double      dval = 0.0;  // float literal
    char        cval = 0;    // char literal
};

// ---------------------------------------------------------------------------
// A compiled function / method
// ---------------------------------------------------------------------------
struct IRFunction {
    std::string name;
    std::string className;   // non-empty for class methods
    std::vector<std::pair<std::string, std::string>> params; // (type, paramName)
    std::vector<IRInstr> code;
};

// ---------------------------------------------------------------------------
// Class metadata kept alongside the IR
// ---------------------------------------------------------------------------
struct IRClass {
    std::string name;
    std::string baseClass;
    std::vector<std::pair<std::string, std::string>> fields; // (type, name)
};

// ---------------------------------------------------------------------------
// The full compiled IR program
// ---------------------------------------------------------------------------
struct IRProgram {
    std::vector<IRInstr>                         main;       // top-level code
    std::unordered_map<std::string, IRFunction>  functions;  // name → function
    std::unordered_map<std::string, IRClass>     classes;    // name → class
};
