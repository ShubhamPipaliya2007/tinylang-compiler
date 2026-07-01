#pragma once
#include "tir.hpp"
#include <string>
#include <vector>
#include <cstdint>

// ---------------------------------------------------------------------------
// Production object model (Phase 4.3 + 4.4 GC).
//
//   • Objects are TLObject*  — raw pointer into TLHeap.
//   • Arrays  are TLArray*   — same.
//   • Values  are TLValue    — typed 8-byte word (tag + payload union).
//
// GC word layout (per ABI doc, abi.md §4.2):
//   bit 0  — mark bit  (set during mark phase, cleared during sweep)
//   bit 1  — pin bit   (reserved: pinned objects are never collected)
//   bits 2+ — reserved for ref-count / generation tag
// ---------------------------------------------------------------------------

// GC bit masks
static constexpr uint64_t GC_MARK_BIT   = 1ULL << 0;
static constexpr uint64_t GC_PINNED_BIT = 1ULL << 1;

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

// ─── Heap allocator + mark-and-sweep GC ─────────────────────────────────────
// Phase 4.4: stop-the-world mark-and-sweep.
//
// The TIRVM calls markValue/markObject/markArray for every root (register,
// alloc-slot, thisObj) in every active frame, then calls sweep() to collect
// unreachable objects.  The mark bit lives in gcWord bit 0.
//
// Pinned objects (gcWord & GC_PINNED_BIT) are never freed by sweep().
// Phase 4.5 will replace the allocator with a bump-pointer arena.

class TLHeap {
public:
    // ── Allocation ────────────────────────────────────────────────────────
    TLObject* allocObject(const std::string& className) {
        auto* obj = new TLObject;
        obj->className = className;
        objects_.push_back(obj);
        ++allocsSinceGC_;
        return obj;
    }

    TLArray* allocArray(const std::string& elemType) {
        auto* arr = new TLArray;
        arr->elemType = elemType;
        arrays_.push_back(arr);
        ++allocsSinceGC_;
        return arr;
    }

    // ── GC threshold ──────────────────────────────────────────────────────
    static constexpr size_t GC_THRESHOLD = 256;
    bool shouldCollect() const {
        return allocsSinceGC_ >= GC_THRESHOLD;
    }
    size_t objectCount() const { return objects_.size() + arrays_.size(); }

    // ── Mark phase (called by TIRVM for each root) ────────────────────────
    void markValue(const TLValue& v) {
        if      (v.isObj()) markObject(v.p.obj);
        else if (v.isArr()) markArray(v.p.arr);
    }

    void markObject(TLObject* obj) {
        if (!obj || (obj->gcWord & GC_MARK_BIT)) return;
        obj->gcWord |= GC_MARK_BIT;
        for (auto& fv : obj->fields) markValue(fv);  // traverse object graph
    }

    void markArray(TLArray* arr) {
        if (!arr || (arr->gcWord & GC_MARK_BIT)) return;
        arr->gcWord |= GC_MARK_BIT;
        for (auto& ev : arr->elements) markValue(ev);
    }

    // ── Sweep phase ───────────────────────────────────────────────────────
    // Deletes unmarked objects, clears marks on survivors.
    // Returns count of freed objects (for stats / testing).
    size_t sweep() {
        size_t freed = 0;

        auto oit = objects_.begin();
        while (oit != objects_.end()) {
            TLObject* obj = *oit;
            if ((obj->gcWord & GC_MARK_BIT) || (obj->gcWord & GC_PINNED_BIT)) {
                obj->gcWord &= ~GC_MARK_BIT;  // clear for next cycle
                ++oit;
            } else {
                delete obj;
                oit = objects_.erase(oit);
                ++freed;
            }
        }

        auto ait = arrays_.begin();
        while (ait != arrays_.end()) {
            TLArray* arr = *ait;
            if ((arr->gcWord & GC_MARK_BIT) || (arr->gcWord & GC_PINNED_BIT)) {
                arr->gcWord &= ~GC_MARK_BIT;
                ++ait;
            } else {
                delete arr;
                ait = arrays_.erase(ait);
                ++freed;
            }
        }

        allocsSinceGC_ = 0;
        totalCollected_ += freed;
        ++gcCycles_;
        return freed;
    }

    // ── Stats ─────────────────────────────────────────────────────────────
    size_t gcCycles()       const { return gcCycles_; }
    size_t totalCollected() const { return totalCollected_; }

    ~TLHeap() {
        for (auto* p : objects_) delete p;
        for (auto* p : arrays_)  delete p;
    }

private:
    std::vector<TLObject*> objects_;
    std::vector<TLArray*>  arrays_;
    size_t allocsSinceGC_  = 0;
    size_t gcCycles_        = 0;
    size_t totalCollected_  = 0;
};
