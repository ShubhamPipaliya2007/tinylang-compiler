# Directory Guide

Quick reference: what belongs where.

## Compiler source code

| Directory                  | What goes here                              |
|----------------------------|---------------------------------------------|
| `compiler/frontend/`       | Lexer, parser, AST, semantic analysis       |
| `compiler/middleend/`      | IR generation, all optimization passes, CFG/SSA |
| `compiler/backend/`        | Bytecode serialization, LLVM codegen (future) |
| `compiler/common/`         | Types shared across all compiler stages (`ir.hpp`) |
| `compiler/cli/`            | `main.cpp` — CLI flags, pipeline wiring     |

## Runtime source code

| Directory           | What goes here                              |
|---------------------|---------------------------------------------|
| `runtime/vm/`       | Stack-based IR interpreter (current runtime)|
| `runtime/heap/`     | Heap allocator (future)                     |
| `runtime/gc/`       | Garbage collector (future)                  |
| `runtime/object/`   | Object model / tagged pointers (future)     |
| `runtime/memory/`   | Page / arena memory management (future)     |
| `runtime/thread/`   | Green threads / scheduler (future)          |

## Standard library

| Directory | What goes here                                    |
|-----------|---------------------------------------------------|
| `stdlib/` | `.tl` source files that ship as the standard library |

Import from your program with a path relative to your file:
```tinylang
import "../stdlib/math_lib.tl";
```

## Tests

| Directory               | What goes here                          |
|-------------------------|-----------------------------------------|
| `tests/semantic/`       | Programs expected to produce specific semantic errors |
| `tests/integration/`    | End-to-end programs that must compile and run |

Run the full suite with `make test`.

## Examples

`examples/` contains runnable `.tl` programs that demonstrate language
features.  They are NOT part of the test suite (they produce output but
are not compared against expected output).

## Documentation

| Directory                       | What goes here                     |
|---------------------------------|------------------------------------|
| `documentation/architecture/`   | System design, data-flow diagrams  |
| `documentation/guides/`         | How-to guides (build, contribute)  |
| `documentation/api/`            | API reference (future)             |

## Infrastructure

| Directory                | What goes here                          |
|--------------------------|-----------------------------------------|
| `infrastructure/ci/`     | GitHub Actions workflow files           |
| `infrastructure/docker/` | Dockerfiles and compose configs         |

## Tools

| Directory                    | What goes here                      |
|------------------------------|-------------------------------------|
| `tools/vscode-extension/`    | VS Code language extension          |
| `tools/package-manager/`     | `tiny` package manager (future)     |
