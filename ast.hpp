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

struct Statement {
    virtual ~Statement() = default;
};

struct Assignment : Statement {
    std::string name;
    std::unique_ptr<Expr> value;
    Assignment(std::string n, std::unique_ptr<Expr> v)
        : name(n), value(std::move(v)) {}
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
