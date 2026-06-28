# Repository Architecture

TinyLang is organized as a professional monorepo. Each top-level directory
is a self-contained module with a single responsibility.

```
tinylang/
├── compiler/           TinyLang compiler (frontend → middleend → backend)
│   ├── frontend/       Lexer, parser, AST, semantic analysis
│   ├── middleend/      IR generation, optimization passes, CFG/SSA
│   ├── backend/        Bytecode writer/reader; LLVM codegen (future)
│   ├── common/         Shared IR type definitions (ir.hpp)
│   └── cli/            Entry point (main.cpp), flag handling, pipeline
│
├── runtime/            TinyLang runtime
│   ├── vm/             Stack-based IR virtual machine
│   ├── heap/           Heap allocator (future)
│   ├── gc/             Garbage collector (future)
│   ├── object/         Object model (future)
│   ├── memory/         Memory management (future)
│   └── thread/         Threading primitives (future)
│
├── stdlib/             TinyLang standard library (.tl source files)
│   ├── math_lib.tl     Math utilities: factorial, power, square, ...
│   ├── string_lib.tl   String utilities: greet, makeMessage, repeat, ...
│   └── shapes_lib.tl   OOP demo: Rectangle, Circle, Triangle classes
│
├── sdk/                Platform SDK stubs (embedded / edge devices)
│   ├── gpio/           General-Purpose I/O
│   ├── camera/         Camera and vision pipeline
│   ├── ai/             AI inference integration
│   ├── network/        Networking
│   └── filesystem/     File system abstraction
│
├── tools/              Developer tooling
│   ├── vscode-extension/  VS Code syntax highlighting + snippets
│   └── package-manager/   Package manager (future)
│
├── operating-system/   TinyLang OS layer (future)
│   ├── kernel-config/  Kernel configuration
│   ├── init/           Init system
│   ├── services/       System services
│   ├── packages/       Package definitions
│   └── installer/      OS installer
│
├── infrastructure/     CI/CD, containers, cloud
│   ├── ci/             GitHub Actions workflows
│   └── docker/         Docker / container configs
│
├── examples/           Runnable .tl example programs
├── tests/              Automated test suite
│   ├── semantic/       Semantic-analysis tests
│   └── integration/    End-to-end integration tests
├── benchmarks/         Performance benchmarks
├── documentation/      Project documentation (this directory)
│   ├── architecture/   System design documents
│   ├── guides/         How-to guides (build, contribute, ...)
│   └── api/            API reference
└── website/            Marketing / documentation website
```

## Design Principles

- **Layered**: Each compiler stage receives a well-defined input and
  produces a well-defined output.  No stage reaches into another.
- **Testable**: Every directory that contains source code has a
  corresponding subtree under `tests/`.
- **Extensible**: Stub directories (heap, gc, sdk, …) define the planned
  growth surface without cluttering active code.
