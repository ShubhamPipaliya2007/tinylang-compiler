#pragma once
#include "tir.hpp"
#include <string>
#include <vector>
#include <cstdint>

// ---------------------------------------------------------------------------
// Production object model (Phase 4.3).
//
// Replaces the handle → unordered_map design:
//   • Objects are TLObject*  — a raw pointer into the TLHeap.
//   • Arrays  are TLArray*   — same.
//   • Values  are TLValue    — a typed 8-byte word (tag + payload).
//
// Memory layout of TLObject mirrors the ABI spec (abi.md):
//   [gcWord 8B][className/vtable placeholder][fieldDefs][fields…]
// Fields are indexed by declaration order, not looked up by name at runtime.
// ---------------------------------------------------------------------------

struct TLObject;
struct TLArray;

// ─── Runtime value ────────────────────────────────────────────────────────────

struct TLValue {
    enum class Tag : uint8_t { Nil, Bool, I32, F64, Char, Str, Obj, Arr } tag = Tag::Nil;

    union Payload {
        int32_t   i;   // I32 and Bool (stored as 0/1)
        double    d;   // F64
        char      c;   // Char
        TLObject* obj; // Obj
        TLArray*  arr; // Arr
        Payload() : i(0) {}
    } p;

    std::string sval; // valid only when tag == Str

    // ── Factories ─────────────────────────────────────────────────────────
    static TLValue nil()                          { return {}; }
    static TLValue fromBool(bool v)               { TLValue r; r.tag=Tag::Bool; r.p.i=v?1:0; return r; }
    static TLValue fromInt(int v)                 { TLValue r; r.tag=Tag::I32;  r.p.i=v;   return r; }
    static TLValue fromFloat(double v)            { TLValue r; r.tag=Tag::F64;  r.p.d=v;   return r; }
    static TLValue fromChar(char v)               { TLValue r; r.tag=Tag::Char; r.p.c=v;   return r; }
    static TLValue fromStr(const std::string& v)  { TLValue r; r.tag=Tag::Str;  r.sval=v;  return r; }
    static TLValue fromObj(TLObject* v)           { TLValue r; r.tag=Tag::Obj;  r.p.obj=v; return r; }
    static TLValue fromArr(TLArray* v)            { TLValue r; r.tag=Tag::Arr;  r.p.arr=v; return r; }

    bool isNil()   const { return tag == Tag::Nil; }
    bool isFloat() const { return tag == Tag::F64; }
    bool isStr()   const { return tag == Tag::Str; }
    bool isObj()   const { return tag == Tag::Obj; }
    bool isArr()   const { return tag == Tag::Arr; }

    bool isTruthy() const {
        switch (tag) {
        case Tag::Nil:          return false;
        case Tag::Bool:
        case Tag::I32:          return p.i != 0;
        case Tag::F64:          return p.d != 0.0;
        case Tag::Char:         return p.c != '\0';
        case Tag::Str:          return !sval.empty();
        case Tag::Obj:          return p.obj != nullptr;
        case Tag::Arr:          return p.arr != nullptr;
        }
        return false;
    }
};

// ─── Heap object ──────────────────────────────────────────────────────────────
// Fields are stored in a contiguous vector indexed by declaration order.
// fieldDefs[i] is the (type, name) of fields[i].
// Both include inherited fields (base first, derived last), matching the
// order produced by TIRVM::collectAllFields().

struct TLObject {
    uint64_t    gcWord    = 0;        // GC mark / pin / tag / refcount (reserved)
    std::string className;            // runtime class name (vtable placeholder)

    std::vector<std::pair<TIR::Type, std::string>> fieldDefs; // (type, name) in order
    std::vector<TLValue>                            fields;   // parallel values

    // Name → field index (-1 if not found).
    int fieldIndex(const std::string& name) const {
        for (int i = 0; i < (int)fieldDefs.size(); ++i)
            if (fieldDefs[i].second == name) return i;
        return -1;
    }

    TLValue getField(const std::string& name) const {
        int idx = fieldIndex(name);
        return (idx >= 0 && idx < (int)fields.size()) ? fields[idx] : TLValue::nil();
    }

    void setField(const std::string& name, const TLValue& v) {
        int idx = fieldIndex(name);
        if (idx >= 0 && idx < (int)fields.size()) fields[idx] = v;
    }
};

// ─── Heap array ───────────────────────────────────────────────────────────────

struct TLArray {
    uint64_t             gcWord   = 0;
    std::string          elemType;
    std::vector<TLValue> elements;
};

// ─── Heap allocator ───────────────────────────────────────────────────────────
// Phase 4.3: ownership via raw pointers tracked in vectors.
// All objects/arrays live until TLHeap is destroyed.
// Phase 4.5 will replace this with a bump-pointer + GC allocator.

class TLHeap {
public:
    TLObject* allocObject(const std::string& className) {
        auto* obj = new TLObject;
        obj->className = className;
        objects_.push_back(obj);
        return obj;
    }

    TLArray* allocArray(const std::string& elemType) {
        auto* arr = new TLArray;
        arr->elemType = elemType;
        arrays_.push_back(arr);
        return arr;
    }

    ~TLHeap() {
        for (auto* p : objects_) delete p;
        for (auto* p : arrays_)  delete p;
    }

private:
    std::vector<TLObject*> objects_;
    std::vector<TLArray*>  arrays_;
};
