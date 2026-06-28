# Contributing Guide

## Getting Started

```bash
git clone https://github.com/ShubhamPipaliya2007/tinylang-compiler.git
cd tinylang-compiler
make          # must build cleanly
make test     # all tests must pass
```

## Branch Strategy

```
main          production-quality; always builds and passes tests
feature/*     new language features
fix/*         bug fixes
refactor/*    structural changes (no behavior change)
docs/*        documentation-only changes
```

## Commit Message Format

```
<scope>: <imperative summary>

Optional body explaining WHY (not what — the diff shows what).
```

Examples:
```
compiler/frontend: add support for string interpolation
runtime/vm: cache method dispatch on first call
compiler/middleend: add SCCP optimization pass
docs: document SSA-based GVN algorithm
```

Scopes match top-level directory names: `compiler`, `runtime`, `stdlib`,
`tools`, `tests`, `docs`, `infrastructure`.

## Adding a Language Feature

1. **Lexer** (`compiler/frontend/lexer.hpp/.cpp`) — add token type.
2. **Parser** (`compiler/frontend/parser.hpp/.cpp`) — add grammar rule, produce AST node.
3. **AST** (`compiler/frontend/ast.hpp`) — add `Statement` / `Expression` subclass.
4. **Semantic** (`compiler/frontend/semantic.hpp/.cpp`) — add type-checking rule.
5. **IRGen** (`compiler/middleend/irgen.hpp/.cpp`) — lower AST node to IR instructions.
6. **VM** (`runtime/vm/irvm.hpp/.cpp`) — add opcode handler if a new `IROp` was added.
7. **Tests** — add a `.tl` file under `tests/integration/` that exercises the feature.
8. **Examples** — optionally add a demo to `examples/`.
9. `make test` — all tests must still pass.

## Adding an Optimization Pass

1. Write the pass as a `static std::vector<IRInstr> myPass(const std::vector<IRInstr>&)` in `compiler/middleend/iropt.cpp`.
2. Wire it into `runOptimizationPasses()` in the same file.
3. Add a test `.tl` file under `tests/integration/` with `--dump-ir` assertions.

## Code Style

- C++17.
- No raw `new`/`delete` — use `unique_ptr` or value types.
- No global mutable state except `g_class_names` (will be removed).
- Comments only where the *why* is non-obvious.
- All warnings must remain at zero after your change.

## Pull Request Checklist

- [ ] `make` succeeds with no new warnings
- [ ] `make test` passes
- [ ] New feature has at least one test in `tests/`
- [ ] Documentation updated if public behavior changed
- [ ] Commit messages follow the format above
