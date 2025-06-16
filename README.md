# TinyLang Compiler

**TinyLang** is a simple toy language compiler and interpreter written in C++17. It was built from scratch to understand core compiler concepts, such as lexical analysis, parsing, AST construction, and execution. TinyLang supports:

### ✅ Features

- Variable declarations: `int x = 5;`
- Arithmetic expressions: `+`, `-`, `*`, `/`
- Logical operators: `==`, `!=`, `<`, `>`
- `print()` for console output
- Conditional statements: `if`, `else`
- `while` loops
- `for` loops
- Function definitions using the `ComeAndDo` keyword
- Function calls
- Return statements
- Expression statements
- VS Code extension for `.tl` syntax highlighting
- Published on the Visual Studio Marketplace
- Data Types: int, bool, String
- Funtion for input

### 🛠 Setup

#### 🔧 Requirements

- C++17 compiler (tested on `g++`)
- `make` (optional, for easier compilation)
- VS Code (for extension usage)

#### 🔁 Build

make


or manually 

g++ -std=c++17 -Wall -o tinylang.exe main.cpp lexer.cpp parser.cpp codegen.cpp
