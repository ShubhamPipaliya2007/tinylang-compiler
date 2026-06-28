# Compiler Architecture

The TinyLang compiler is a classic multi-pass pipeline:

```
Source (.tl)
    │
    ▼
┌─────────────────────────────────────────┐
│  FRONTEND  (compiler/frontend/)          │
│  Lexer → Tokens → Parser → AST          │
│  Semantic analysis + type checking       │
└───────────────────┬─────────────────────┘
                    │  std::vector<Statement*>
                    ▼
┌─────────────────────────────────────────┐
│  MIDDLEEND  (compiler/middleend/)        │
│  IRGen   — AST → flat IRProgram          │
│  Passes 1-8 (optimization pipeline):     │
│    1. Constant propagation               │
│    2. Dead code elimination              │
│    3. Copy propagation                   │
│    4. Dead store elimination             │
│    5. Common subexpression elimination   │
│    6. Strength reduction                 │
│    7. Loop-invariant code motion         │
│    8. SSA-based Global Value Numbering   │
│  CFG / Liveness DSE / SSA / Dominators  │
└───────────────────┬─────────────────────┘
                    │  IRProgram (optimized)
          ┌─────────┴─────────┐
          │                   │
          ▼                   ▼
┌──────────────────┐  ┌──────────────────────┐
│ BACKEND          │  │ RUNTIME              │
│ (compiler/       │  │ (runtime/vm/)        │
│  backend/)       │  │                      │
│ Bytecode .tlc    │  │ Stack-based VM        │
│ LLVM (future)    │  │ Heap / GC (future)   │
└──────────────────┘  └──────────────────────┘
```

## Shared Types — compiler/common/ir.hpp

`ir.hpp` is the contract between every compiler stage.  It defines:

| Type         | Purpose                                         |
|--------------|------------------------------------------------|
| `IROp`       | Enum of all IR opcodes (PUSH_INT, LOAD, ADD …) |
| `IRInstr`    | One flat instruction: opcode + sval + ival + …  |
| `IRFunction` | Named function: params + code vector            |
| `IRClass`    | Class descriptor: fields + base class           |
| `IRProgram`  | Full program: classes + functions + main        |

## IR Instruction Format

Every instruction is fixed-width on disk (18 bytes):

```
[1B opcode][4B sval_pool_idx][4B ival][8B dval][1B cval]
```

## Optimization Pipeline (compiler/middleend/)

All 8 passes operate in `runOptimizationPasses()` in `iropt.cpp`.
Passes 1–7 are flat-list scans.  Pass 8 (GVN) builds a CFG, constructs
SSA form via Cytron et al., runs a dominator-tree DFS to eliminate
cross-block redundancies, then destroys SSA back to flat IR.

### CFG Infrastructure (cfg.hpp / cfg.cpp)

| Component           | Algorithm           |
|---------------------|---------------------|
| Basic blocks        | Leader heuristic    |
| Dominators          | Cooper et al. (2001)|
| Dominance frontiers | Cytron et al.       |
| Liveness            | Backward worklist   |
| SSA construction    | Cytron et al. φ-insertion + rename |
| SSA destruction     | Briggs lost-copy    |

## Bytecode Format — compiler/backend/bytecode.hpp

Binary `.tlc` files allow ahead-of-time compilation:

```
[4B magic: TLBC][2B version][string pool][class table][function table][main]
```

The VM reads `.tlc` directly, skipping the entire frontend and middleend.
