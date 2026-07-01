# TinyLang ABI Design

**Phase 4.2 — Pre-LLVM ABI specification.**  
This document defines how TinyLang functions communicate at the binary
level: parameter passing, return values, stack layout, and object memory
layout.  Nothing here requires code changes today; it is the contract
that the LLVM backend must satisfy.

---

## 1. Goals

| Goal | Rationale |
|---|---|
| Map cleanly to LLVM IR calling conventions | Avoid shim layers when lowering TIR → LLVM IR |
| Match the platform C ABI at boundaries | Required for FFI (`extern` calls to C libraries) |
| Uniform across ARM64 and x86-64 | Primary embedded target is ARM64; dev machines are x86-64 |
| Object layout compatible with a future GC | Header must carry mark bits without touching field offsets |

---

## 2. The TinyLang Calling Convention (TinyCC)

TinyCC is a **register-first** calling convention.  Small integers,
booleans, chars, and pointers go in integer registers; floats go in
float registers; excess arguments spill to the stack.  TinyCC is
intentionally isomorphic to the platform C ABI so that calling a
`extern` C function requires no translation.

### 2.1 Integer / pointer argument registers

| Slot | ARM64 register | x86-64 register | Holds |
|---|---|---|---|
| arg 0 | `x0` | `rdi` | first i32 / i1 / char / object pointer |
| arg 1 | `x1` | `rsi` | |
| arg 2 | `x2` | `rdx` | |
| arg 3 | `x3` | `rcx` | |
| arg 4 | `x4` | `r8` | |
| arg 5 | `x5` | `r9` | (x86-64 limit: 6 integer regs) |
| arg 6 | `x6` | stack | |
| arg 7 | `x7` | stack | (ARM64 limit: 8 integer regs) |
| arg N≥8 | stack | stack | right-to-left, 8-byte aligned |

### 2.2 Float argument registers

| Slot | ARM64 register | x86-64 register |
|---|---|---|
| float 0 | `v0` | `xmm0` |
| float 1 | `v1` | `xmm1` |
| … | … | … |
| float 7 | `v7` | `xmm7` |
| float N≥8 | stack | stack |

Float and integer slots are counted **independently**.  A call
`foo(int, float, int)` uses `x0`, `v0`, `x1` — `v0` does not
consume an integer slot.

### 2.3 The `this` pointer

For method calls, `this` is passed as the **zeroth** argument in `x0` /
`rdi`, before any declared parameters.  This matches the Itanium C++ ABI
and what LLVM emits for member functions.

```
// TinyLang:  s.greet(name)
// Wire:      greet(this=&s, name)    →  x0=&s, x1=&name_str
```

### 2.4 Return values

| TinyLang type | Return location | Notes |
|---|---|---|
| `void` | — | no return value |
| `bool`, `char`, `int` | `x0` / `rax` | zero-extended to 64 bits |
| `float` | `v0` / `xmm0` | full 64-bit double |
| object reference | `x0` / `rax` | pointer to heap object |
| array reference | `x0` / `rax` | pointer to heap array |
| struct ≤ 16 bytes | `x0:x1` / `rax:rdx` | two registers, packed |
| struct > 16 bytes | via hidden pointer | caller allocates; pointer in `x8` / `rdi` (sret) |

### 2.5 Stack alignment

- The stack pointer **must be 16-byte aligned** at every `CALL`
  instruction on both ARM64 and x86-64.
- On x86-64, the `CALL` instruction pushes 8 bytes (return address),
  leaving SP 8-byte aligned inside the callee prologue.  The prologue
  must push an even number of 8-byte words to restore 16-byte alignment.
- On ARM64, the return address is stored in `lr`; the prologue saves
  `fp, lr` together (16 bytes) to maintain alignment.

### 2.6 Callee-saved vs caller-saved registers

| Category | ARM64 | x86-64 |
|---|---|---|
| Caller-saved (scratch) | `x0–x17`, `v0–v7` | `rax,rcx,rdx,rsi,rdi,r8–r11`,`xmm0–xmm15` |
| Callee-saved (preserved) | `x19–x28`, `v8–v15` | `rbx,rbp,r12–r15` |
| Special | `x29`=FP, `x30`=LR, `x31`=SP | `rsp`=SP, `rbp`=FP |

A TinyLang function must save and restore any callee-saved register it
uses.  The LLVM backend handles this automatically; it matters for any
hand-written assembly stubs (GC write barriers, runtime calls).

---

## 3. Stack Frame Layout

Frames grow **downward** (toward lower addresses), standard for ARM64
and x86-64.

```
High addresses (caller's SP before call)
┌──────────────────────────────────────┐
│  ...  caller's frame  ...            │
│                                      │
│  spilled arg N   (8 bytes, aligned)  │  ← only if args > register limit
│  spilled arg 8                       │
├──────────────────────────────────────┤  ← SP at CALL instruction
│  return address  (x86: pushed by CALL│
│                   ARM: saved in LR)  │
│  saved frame pointer (x29 / rbp)     │  8 or 16 bytes
├──────────────────────────────────────┤  ← FP (frame pointer)
│  callee-saved registers              │  variable, aligned
│  (x19–x28 / rbx r12–r15)            │
├──────────────────────────────────────┤
│  local alloca slots                  │  one slot per TIR `alloc`
│    slot for "x"  (8 bytes)           │
│    slot for "y"  (8 bytes)           │
│    ...                               │
├──────────────────────────────────────┤
│  spill area for temporaries          │  register allocator controlled
├──────────────────────────────────────┤
│  outgoing args (spilled)             │  for calls made from this frame
└──────────────────────────────────────┘  ← SP (callee's stack pointer)
Low addresses
```

**Notes**

- Every TIR `alloc` instruction becomes a fixed-size slot in the alloca
  area.  Size is determined by type: i1/char=1B padded to 8B, i32=4B
  padded to 8B, f64=8B, pointer=8B.
- Slot addresses are known at compile time (FP-relative offsets).  A
  `load %r %slot` becomes a single `ldr` / `mov` from the stack.
- The TIR alloc/load/store model maps to LLVM `alloca` / `load` /
  `store` instructions directly; LLVM's `mem2reg` pass will promote most
  of these to pure SSA registers automatically.

---

## 4. Object Layout

### 4.1 Current model (TIRVM)

Objects are identified by a **string handle** (`"h0"`, `"h1"` …) and
stored as `unordered_map<string, IRValue>`.  This is fine for the
interpreter but cannot be lowered to native code.

### 4.2 Target model (native runtime)

Every heap object is a **contiguous block** in managed memory:

```
Object pointer (held by a variable or field)
│
▼
┌─────────────────────────────────────────┐  ← object base address
│  GC word  (8 bytes)                     │
│  ┌─────────────────────────────────────┐│
│  │  mark (1 bit)   — GC live mark      ││
│  │  pinned (1 bit) — must not move     ││
│  │  tag (2 bits)   — OBJ / ARR / STR  ││
│  │  reserved (12 bits)                 ││
│  │  refcount (48 bits) — optional RC   ││
│  └─────────────────────────────────────┘│
├─────────────────────────────────────────┤  +8
│  vtable*  (8 bytes)                     │  → see §4.3
├─────────────────────────────────────────┤  +16  ← start of fields
│  field 0  (8 bytes)                     │
│  field 1  (8 bytes)                     │
│  field 2  (8 bytes)                     │
│  ...                                    │
└─────────────────────────────────────────┘
```

- **Total header size: 16 bytes.**  Field 0 is always at `obj + 16`.
- Fields are laid out in **declaration order**, matching the order in
  `TIR::Class::fields`.  The compiler assigns fixed offsets; no
  dictionary lookup at runtime.
- Each field is **8 bytes wide** regardless of logical type (see §4.5
  for how values are encoded in 8 bytes).

### 4.3 VTable

Every class has one vtable, shared by all instances, stored in
read-only memory.

```
VTable for class "Dog":
┌──────────────────────────────────────┐
│  class_name*  → "Dog\0"  (pointer)   │
│  base_vtable* → VTable<Animal>       │  null for root classes
│  sizeof_obj   (4 bytes)              │  total object size in bytes
│  num_fields   (4 bytes)              │
│  field_names* → ["name","age",...]   │  for reflection / debugger
├──────────────────────────────────────┤
│  method 0:  init     →  Dog::init    │  function pointer
│  method 1:  speak    →  Dog::speak   │
│  method 2:  toString →  Animal::toString  (inherited, not overridden)
│  ...                                 │
└──────────────────────────────────────┘
```

Method dispatch:
```
obj->vtable->method[N](obj, arg0, arg1, ...)
```

The method index `N` is resolved at **compile time** (not runtime
dictionary lookup).  For virtual dispatch the compiler emits an indirect
call through the vtable; for non-virtual calls (static dispatch) it
emits a direct call.

Inheritance:  a subclass vtable is a superset of the base vtable.  The
first `M` slots of a subclass vtable are identical to the base vtable,
so a base-class pointer can safely dereference the vtable.

### 4.4 Field offsets

Given a class with fields declared as:
```
class Person {
    name: string
    age:  int
}
```
The object layout is:

```
offset  0:  GC word
offset  8:  vtable*
offset 16:  name   (8-byte value word — holds pointer to string object)
offset 24:  age    (8-byte value word — holds i32 in low 32 bits)
```

Field offsets are computed by `TIRGen` during the first pass
(`firstPass`) when class definitions are processed, and stored in a
`field_offset_map` alongside the existing `TIR::Class::fields`.

### 4.5 Value representation (the 8-byte word)

Every field and local variable is stored as an **8-byte tagged word**:

```
Option A — NaN-boxing (used by JavaScriptCore, LuaJIT)
  • A valid IEEE 754 double stores a float64 directly.
  • Any bit pattern where exponent = 0x7FF and mantissa ≠ 0 is a NaN.
  • The 48-bit mantissa payload encodes: [type tag 4 bits][payload 44 bits].
  • Integers, booleans, pointers fit in 44 bits on ARM64 / x86-64.

Option B — Tagged union word (simpler, chosen for TinyLang)
  ┌────────────────────────────────────────────────────┐
  │  type_tag (8 bits)  │  payload (56 bits)           │
  └────────────────────────────────────────────────────┘
```

**TinyLang uses Option B** for clarity and 32-bit target compatibility.

Type tags:
| Tag (hex) | Logical type | Payload |
|---|---|---|
| `0x00` | nil / void | unused |
| `0x01` | i1 (bool) | 0 or 1 in low bit |
| `0x02` | i32 (int) | 32-bit signed int in low 32 bits |
| `0x03` | f64 (float) | full 64-bit double (whole word, no tag bits*) |
| `0x04` | char | Unicode code point in low 32 bits |
| `0x05` | str | pointer to String object in low 48 bits |
| `0x06` | obj | pointer to heap Object in low 48 bits |
| `0x07` | arr | pointer to heap Array in low 48 bits |

*f64 uses a separate 8-byte word with the tag stored in an adjacent
nibble or a separate type descriptor, to avoid corrupting the float
bits.  For struct fields, the type is known statically and the tag word
is omitted entirely — the compiler uses an unboxed `double` directly.

### 4.6 Array layout

```
┌─────────────────────────────────────────┐
│  GC word  (8 bytes)                     │
├─────────────────────────────────────────┤
│  vtable*  → VTable<__Array>  (8 bytes)  │
├─────────────────────────────────────────┤
│  length   (8 bytes, i64)                │
│  capacity (8 bytes, i64)                │
│  elem_tag (1 byte)  │ reserved (7 bytes)│  element type tag
├─────────────────────────────────────────┤  ← elements start here
│  element 0  (8 bytes)                   │
│  element 1  (8 bytes)                   │
│  ...                                    │
└─────────────────────────────────────────┘
```

An array of `int` with known element type can omit the per-element tag
and store raw `i32` values (4 bytes each) — a compiler optimisation for
typed arrays.  Untyped or mixed arrays use 8-byte tagged words.

### 4.7 String layout

Strings are **immutable** heap objects.  They are UTF-8 encoded.

```
┌─────────────────────────────────────────┐
│  GC word  (8 bytes)                     │
├─────────────────────────────────────────┤
│  vtable*  → VTable<__String>  (8 bytes) │
├─────────────────────────────────────────┤
│  length   (8 bytes — byte count, not codepoints) │
│  hash     (8 bytes — cached FNV-1a hash)│
├─────────────────────────────────────────┤
│  UTF-8 bytes  (length bytes, null-terminated) │
│  padding to 8-byte boundary             │
└─────────────────────────────────────────┘
```

String interning (deduplication) is optional and deferred to the
standard library.

---

## 5. Name Mangling

TinyLang uses a simple mangling scheme so that methods from different
classes do not collide in the symbol table.

| TinyLang | Mangled symbol |
|---|---|
| free function `foo(int, float)` | `_TL_foo__i_f` |
| `Person::init(string)` | `_TL_Person__init__s` |
| `Person::greet()` | `_TL_Person__greet__` |
| `Animal::speak(int)` | `_TL_Animal__speak__i` |

Type suffixes: `v`=void, `i`=int, `f`=float, `b`=bool, `c`=char,
`s`=str, `pX`=pointer to class X, `aX`=array of X.

The TIR already uses `"ClassName::methodName"` as its key; the mangler
just prepends `_TL_` and encodes the parameter types.  LLVM IR uses the
mangled name as the function's `@` symbol.

---

## 6. C FFI

`extern` declarations in TinyLang call C functions with no wrapper.
Because TinyCC is isomorphic to the platform C ABI, the only
obligation is type mapping:

| TinyLang type | C type |
|---|---|
| `int` | `int32_t` |
| `float` | `double` |
| `bool` | `_Bool` / `int` (0 or 1) |
| `char` | `uint32_t` (Unicode code point) |
| `string` | `const char*` (pointer to UTF-8 bytes inside String object) |
| object | `void*` (pointer to heap object base) |

When passing a `string` to a C function, the runtime must extract the
UTF-8 byte pointer (`obj + 32`) from the String object.  A built-in
intrinsic `__tl_str_cptr(s)` provides this.

---

## 7. Transition Plan

This ABI is not yet implemented.  It becomes relevant in these phases:

| Phase | ABI concern |
|---|---|
| **4.2** (this doc) | Design only — no code changes |
| **4.3** (LLVM backend) | `TIRGen` must emit LLVM IR respecting TinyCC: `call` with correct arg types, `alloca` for local slots, `getelementptr` for field access |
| **4.4** (native objects) | `TIRVM` heap transitions from `unordered_map<string, IRValue>` to the contiguous layout in §4.2; vtable dispatch replaces `findMethodCached` |
| **4.5** (GC) | GC word in §4.2 becomes live; write barriers inserted at `StoreField` and `StoreArr` sites |

Until Phase 4.3, the TIRVM continues to use handles and `unordered_map`
storage.  The ABI document records the **target** state, not the current
state.
