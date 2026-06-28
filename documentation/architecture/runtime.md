# Runtime Architecture

## Current VM — runtime/vm/

The current runtime is a stack-based IR interpreter (`irvm.hpp / irvm.cpp`).

### VMFrame

Each function invocation creates a `VMFrame`:

```cpp
struct VMFrame {
    vector<unordered_map<string, IRValue>> scopes; // scope stack
    string className;    // executing method's class
    string thisHandle;   // "this" object handle
};
```

Scope operations:
- `ENTER_SCOPE` → `pushScope()` (inner variable block)
- `EXIT_SCOPE`  → `popScope()`
- `DECLARE x`  → `scopes.back()[x] = val` (always innermost)
- `STORE x`    → walk from innermost outward; fallback to `scopes[0]`
- `LOAD x`     → walk from innermost outward

The fallback for `STORE` targets `scopes[0]` (outermost) rather than
`scopes.back()` so that SSA-generated temporaries written inside a nested
scope survive the corresponding `EXIT_SCOPE`.

### Object Model

Objects live in `objHeap_` (an `unordered_map<string, IRObject>`).
Each object holds a `className` and a field map.

Methods use the **fields-as-locals** pattern: all object fields are
copied into `frame.scopes[0]` on method entry and synced back on exit.

### Method Dispatch Cache

`findMethodCached(cls, method)` caches resolution results in
`methodCache_` (`unordered_map<string, const IRFunction*>`), keyed by
`"ClassName::methodName"`.  On first call the inheritance chain is walked
(constant-time per level); all subsequent calls for the same pair are
direct pointer lookups.

### IRValue

```
type: NIL | INT | FLOAT | CHAR | STRING | OBJ_HANDLE | ARR_HANDLE
i   : int    (INT, BOOL)
f   : double (FLOAT)
c   : char   (CHAR)
s   : string (STRING text, or heap handle for OBJ/ARR)
```

## Planned Runtime Modules

| Module          | Responsibility                            |
|-----------------|-------------------------------------------|
| `runtime/heap/` | Bump-pointer + free-list allocator        |
| `runtime/gc/`   | Mark-and-sweep / incremental GC           |
| `runtime/object/` | Tagged-pointer object representation    |
| `runtime/memory/` | Page allocator, arena management        |
| `runtime/thread/` | Green threads / cooperative scheduler   |

The transition from the current interpreter to the planned native
runtime will proceed alongside the LLVM backend in `compiler/backend/`.
