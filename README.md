# TinyLang

An AI-first embedded computing language and compiler, built in C++17 from scratch.

[![CI](https://github.com/ShubhamPipaliya2007/tinylang-compiler/actions/workflows/ci.yml/badge.svg)](https://github.com/ShubhamPipaliya2007/tinylang-compiler/actions/workflows/ci.yml)

## What's in the box

| Component | Status | Location |
|-----------|--------|----------|
| Lexer + Parser | ✅ | `compiler/frontend/` |
| AST | ✅ | `compiler/frontend/ast.hpp` |
| Semantic analysis + type checking | ✅ | `compiler/frontend/semantic.*` |
| IR (SSA / CFG / Dominator tree / Liveness) | ✅ | `compiler/middleend/` |
| 8-pass optimizer (CP, DCE, CSE, LICM, GVN…) | ✅ | `compiler/middleend/iropt.*` |
| Bytecode (`.tlc`) — compile once, run many | ✅ | `compiler/backend/` |
| Stack VM with method-dispatch cache | ✅ | `runtime/vm/` |
| OOP — classes, inheritance, constructors, `super` | ✅ | language feature |
| Module system (`import "path.tl"`) | ✅ | language feature |
| Arrays (all types, object arrays) | ✅ | language feature |
| VS Code extension | ✅ | `tools/vscode-extension/` |
| Standard library | 🔨 | `stdlib/` |
| LLVM backend | 🗺 | `compiler/backend/` |
| Garbage collector | 🗺 | `runtime/gc/` |
| Package manager | 🗺 | `tools/package-manager/` |
| Platform SDK (GPIO / camera / AI / network) | 🗺 | `sdk/` |

## Quick Start

```bash
git clone https://github.com/ShubhamPipaliya2007/tinylang-compiler.git
cd tinylang-compiler
make
./tinylang examples/sample.tl
```

## Language at a glance

```tinylang
// Variables
int x = 42;
float pi = 3.14;
string msg = "hello";
bool ok = true;

// Functions (ComeAndDo keyword)
ComeAndDo factorial(int n) {
    if (n <= 1) { return 1; }
    return n * factorial(n - 1);
}

// OOP
class Animal {
    string name;
    ComeAndDo speak() { print(name + " speaks!"); }
}
class Dog : Animal {
    ComeAndDo speak() { print(name + " barks!"); }
}

Dog d("Rex");
d.speak();   // Rex barks!

// Modules
import "../stdlib/math_lib.tl";
print(factorial(10));
```

## Build

```bash
make              # build compiler
make test         # run test suite
make examples     # run all examples
make clean        # remove binary
```

Run with flags:

```bash
./tinylang file.tl              # compile + run
./tinylang file.tl --dump-ir   # show optimized IR
./tinylang file.tl --dump-cfg  # show CFG + liveness + dominators
./tinylang file.tl --compile   # write file.tlc (bytecode)
./tinylang file.tlc            # run pre-compiled bytecode
```

## Repository Layout

```
compiler/   frontend / middleend / backend / common / cli
runtime/    vm + planned heap / gc / object / memory / thread
stdlib/     TinyLang standard library (.tl source)
sdk/        Platform SDK stubs (GPIO, camera, AI, network)
tools/      VS Code extension, package manager (future)
operating-system/  OS layer (future)
infrastructure/    CI, Docker
examples/   Runnable demo programs
tests/      Automated test suite
documentation/  Architecture + guides
```

Full docs: [documentation/README.md](documentation/README.md)

## Contributing

See [documentation/guides/contributing.md](documentation/guides/contributing.md).

## License

[LICENSE](LICENSE)
