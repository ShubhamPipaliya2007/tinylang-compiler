# TinyLang Compiler

**TinyLang** is a lightweight toy compiler written in modern C++17 for a custom language with minimal syntax. This language includes variable declarations, function definitions (with the custom keyword `ComeAndDo`), conditionals, and simple expressions.

---

## 🚀 Features

- ✅ Lexical analysis (tokenizer)
- ✅ Recursive descent parser
- ✅ AST (Abstract Syntax Tree) generation
- ✅ Function definitions using `ComeAndDo`
- ✅ Conditionals: `if`, `else`
- ✅ Arithmetic operations: `+`, `-`, `*`, `/`
- ✅ Comparison operators: `==`, `!=`, `<`, `>`
- ✅ Function calls and return statements
- ✅ Printing output via `print()`

---

## 📜 Example Code (in `sample.tl`)

```tl
ComeAndDo check(x) {
    if (x > 0) {
        print(1);
    } else {
        print(0);
    }
}

check(5);
