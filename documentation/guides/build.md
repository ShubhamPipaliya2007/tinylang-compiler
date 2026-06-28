# Build Guide

## Prerequisites

| Tool  | Minimum version | Purpose            |
|-------|-----------------|--------------------|
| g++   | 10.0            | C++17 compiler     |
| make  | 3.81            | Build system       |
| git   | 2.30            | Version control    |

CMake support is planned but not yet wired.

## Quick Start

```bash
git clone https://github.com/ShubhamPipaliya2007/tinylang-compiler.git
cd tinylang-compiler
make
./tinylang examples/sample.tl
```

## Build Targets

| Target          | Command        | Description                          |
|-----------------|----------------|--------------------------------------|
| Build compiler  | `make`         | Produces `./tinylang` binary         |
| Run all tests   | `make test`    | Semantic + integration test suite    |
| Run examples    | `make examples`| Executes every `.tl` in `examples/`  |
| Clean           | `make clean`   | Remove compiled binary               |

## Include Search Paths

The Makefile adds these `-I` flags so all `#include "name.hpp"` directives
resolve without modification regardless of which subdirectory a file is in:

```
-Icompiler/frontend
-Icompiler/middleend
-Icompiler/backend
-Icompiler/common
-Iruntime/vm
```

## Runtime Flags

| Flag           | Effect                                           |
|----------------|--------------------------------------------------|
| `--dump-ir`    | Print optimized flat IR before execution         |
| `--dump-cfg`   | Print CFG with liveness and dominator info       |
| `--compile`    | Write `.tlc` bytecode file and exit              |

## Compile Once, Run Many Times

```bash
# Compile to bytecode
./tinylang examples/sample.tl --compile
# Produces examples/sample.tlc

# Execute bytecode directly (skips the entire compiler pipeline)
./tinylang examples/sample.tlc
```

## Adding a New Source File

1. Place the file in the appropriate subdirectory
   (`compiler/frontend/`, `compiler/middleend/`, …).
2. Add it to `SRC` in `Makefile`.
3. Add any new header to `HEADERS` in `Makefile`.
4. `make` to verify it builds.
