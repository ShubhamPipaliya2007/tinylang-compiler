# TinyLang Compiler

**TinyLang** is a simple toy language compiler and interpreter written in C++17. It was built from scratch to understand core compiler concepts, such as lexical analysis, parsing, AST construction, and execution. TinyLang supports:

### ✅ Features

- Variable declarations: `int x = 5;`, `float y = 3.14;`, `char c = 'A';`, `bool flag = true;`, `string s = "hello";`
- String variable assignment, printing, and concatenation: `string s2 = s1 + ", " + s3;`, `print(s2);`
- Local and global variable scoping: Variables declared inside functions are local, outside are global
- Single-line (`// ...`) and multi-line (`/* ... */`) comments
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
- **Arrays:** fixed-size and initialized arrays for all types (e.g., `int arr[10];`, `float nums[] = {1.1, 2.2};`, `char letters[] = {'A', 'B'};`)
- Function for input: `input()`
- **Improved error messages** with line/column info
- **VS Code extension:**
- Syntax highlighting for all language features
- Code snippets for common patterns (function, loop, print, etc.)
- Available on the Visual Studio Marketplace

### 🛠 Setup

#### 🔧 Requirements

- C++17 compiler (tested on `g++`)
- `make` (optional, for easier compilation)
- VS Code (for extension usage)

#### 🔁 Build

make

or manually

g++ -std=c++17 -Wall -o tinylang.exe main.cpp lexer.cpp parser.cpp codegen.cpp

#### ▶️ Run

./tinylang.exe yourfile.tl

### 💡 Example

```tl
// This is a single-line comment
/*
   This is a multi-line comment
   It can span multiple lines
*/
int arr[] = {1, 2, 3, 4, 5};
arr[2] = 42;
print(arr[2]);

float nums[3];
nums[0] = 3.14;
nums[1] = 2.71;
nums[2] = nums[0] + nums[1];
print(nums[2]);

char letters[] = {'A', 'B', 'C'};
print(letters[1]);
bool flags[] = {true, false, true};
flags[1] = true;
print(flags[1]);

string words[] = {"hello", "world"};
print(words[0]);

# String variable assignment and concatenation
string s1 = "Hello";
string s2 = "World";
string s3 = s1 + ", " + s2;
print(s3);
string s4 = s3;
print(s4);
print("Done!");

# Local and global variable scoping
int x = 100;
print(x); // Prints 100 (global)

ComeAndDo testScope() {
    int x = 42;
    print(x); // Prints 42 (local)
}

testScope();
print(x); // Prints 100 (global again)
```
