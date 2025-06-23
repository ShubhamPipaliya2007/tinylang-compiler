#pragma once
#include <string>
#include <memory>
#include <vector>
#include "lexer.hpp"

struct Expr {
    virtual ~Expr() = default;
};

struct Number : Expr {
    int value;
    Number(int v) : value(v) {}
};

struct BoolLiteral : Expr {
    bool value;
    BoolLiteral(bool v) : value(v) {}
};

struct StringLiteral : Expr {
    std::string value;
    StringLiteral(std::string v) : value(std::move(v)) {}
};

struct Variable : Expr {
    std::string name;
    Variable(std::string n) : name(n) {}
};

struct BinaryExpr : Expr {
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    TokenType op;

    BinaryExpr(std::unique_ptr<Expr> l, TokenType o, std::unique_ptr<Expr> r)
        : left(std::move(l)), right(std::move(r)), op(o) {}
};

struct UnaryExpr : Expr {
    std::unique_ptr<Expr> operand;
    TokenType op;

    UnaryExpr(TokenType o, std::unique_ptr<Expr> operand)
        : operand(std::move(operand)), op(o) {}
};

// ➕ NEW: input()
struct InputExpr : Expr {
    InputExpr() = default;
};

// ➕ NEW: read("filename")
struct ReadExpr : Expr {
    std::string filename;
    ReadExpr(std::string f) : filename(std::move(f)) {}
};

struct Statement {
    virtual ~Statement() = default;
};

struct Assignment : Statement {
    std::string name;
    std::unique_ptr<Expr> value;
    std::string type;
    Assignment(std::string n, std::unique_ptr<Expr> v, std::string t = "")
        : name(std::move(n)), value(std::move(v)), type(std::move(t)) {}
};

struct Print : Statement {
    std::unique_ptr<Expr> value;
    Print(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

struct FunctionDef : public Statement {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<std::unique_ptr<Statement>> body;

    FunctionDef(std::string name, std::vector<std::string> params,
                std::vector<std::unique_ptr<Statement>> body)
        : name(std::move(name)), parameters(std::move(params)), body(std::move(body)) {}
};

struct Return : Statement {
    std::unique_ptr<Expr> value;
    Return(std::unique_ptr<Expr> v) : value(std::move(v)) {}
};

struct CallExpr : public Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(std::string name, std::vector<std::unique_ptr<Expr>> args)
        : callee(std::move(name)), arguments(std::move(args)) {}
};

struct IfStatement : Statement {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Statement>> thenBranch;
    std::vector<std::unique_ptr<Statement>> elseBranch;

    IfStatement(std::unique_ptr<Expr> cond,
                std::vector<std::unique_ptr<Statement>> thenB,
                std::vector<std::unique_ptr<Statement>> elseB)
        : condition(std::move(cond)),
          thenBranch(std::move(thenB)),
          elseBranch(std::move(elseB)) {}
};

struct WhileStatement : Statement {
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Statement>> body;

    WhileStatement(std::unique_ptr<Expr> cond, std::vector<std::unique_ptr<Statement>> body)
        : condition(std::move(cond)), body(std::move(body)) {}
};

struct ForStatement : Statement {
    std::unique_ptr<Statement> initializer;
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Statement> increment;
    std::vector<std::unique_ptr<Statement>> body;

    ForStatement(std::unique_ptr<Statement> init,
                 std::unique_ptr<Expr> cond,
                 std::unique_ptr<Statement> incr,
                 std::vector<std::unique_ptr<Statement>> body)
        : initializer(std::move(init)), condition(std::move(cond)),
          increment(std::move(incr)), body(std::move(body)) {}
};

class ExprStatement : public Statement {
    public:
        std::unique_ptr<Expr> expr;

        ExprStatement(std::unique_ptr<Expr> expr)
            : expr(std::move(expr)) {}
};

struct FloatLiteral : Expr {
    double value;
    FloatLiteral(double v) : value(v) {}
};

struct CharLiteral : Expr {
    char value;
    CharLiteral(char v) : value(v) {}
};

struct ArrayLiteral : Expr {
    std::vector<std::unique_ptr<Expr>> elements;
    ArrayLiteral(std::vector<std::unique_ptr<Expr>> elems) : elements(std::move(elems)) {}
};

struct ArrayAccess : Expr {
    std::string arrayName;
    std::unique_ptr<Expr> index;
    ArrayAccess(std::string name, std::unique_ptr<Expr> idx) : arrayName(std::move(name)), index(std::move(idx)) {}
};

struct ArrayAssignment : Statement {
    std::string arrayName;
    std::unique_ptr<Expr> index;
    std::unique_ptr<Expr> value;
    ArrayAssignment(std::string name, std::unique_ptr<Expr> idx, std::unique_ptr<Expr> val)
        : arrayName(std::move(name)), index(std::move(idx)), value(std::move(val)) {}
};

// AST node for class definition
struct ClassDef : Statement {
    std::string name;
    std::vector<std::pair<std::string, std::string>> fields; // (type, name)
    std::vector<std::unique_ptr<FunctionDef>> methods;
    ClassDef(std::string n,
             std::vector<std::pair<std::string, std::string>> f,
             std::vector<std::unique_ptr<FunctionDef>> m)
        : name(std::move(n)), fields(std::move(f)), methods(std::move(m)) {}
};

// AST node for object member access (e.g., obj.field or obj.method())
struct ObjectMemberAccess : Expr {
    std::unique_ptr<Expr> object;
    std::string member;
    ObjectMemberAccess(std::unique_ptr<Expr> obj, std::string mem)
        : object(std::move(obj)), member(std::move(mem)) {}
};

// AST node for object method call (e.g., obj.method(args...))
struct ObjectMethodCall : Expr {
    std::unique_ptr<Expr> object;
    std::string method;
    std::vector<std::unique_ptr<Expr>> arguments;
    ObjectMethodCall(std::unique_ptr<Expr> obj, std::string m, std::vector<std::unique_ptr<Expr>> args)
        : object(std::move(obj)), method(std::move(m)), arguments(std::move(args)) {}
};

