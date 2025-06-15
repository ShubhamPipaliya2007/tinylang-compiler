#include "ast.hpp"
#include <iostream>
#include <unordered_map>

static std::unordered_map<std::string, int> variables;
static std::unordered_map<std::string, const FunctionDef*> functions;

// Forward declaration
void execute(const Statement* stmt);

int evalExpr(const Expr* expr) {
    if (auto num = dynamic_cast<const Number*>(expr)) {
        return num->value;
    } else if (auto var = dynamic_cast<const Variable*>(expr)) {
        if (variables.count(var->name)) return variables[var->name];
        throw std::runtime_error("Undefined variable: " + var->name);
    } else if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
        int left = evalExpr(bin->left.get());
        int right = evalExpr(bin->right.get());

        switch (bin->op) {
            case TokenType::PLUS: return left + right;
            case TokenType::MINUS: return left - right;
            case TokenType::MULTIPLICATION: return left * right;
            case TokenType::DIVISION:
                if (right == 0) throw std::runtime_error("Division by zero is not allowed");
                return left / right;
            case TokenType::GREATERTHEN: return left > right;
            case TokenType::LESSTHEN: return left < right;
            case TokenType::EQUALTO: return left == right;
            case TokenType::NOTEQUALTO: return left != right;
            default: throw std::runtime_error("Unsupported binary operator");
        }
    } else if (auto call = dynamic_cast<const CallExpr*>(expr)) {
        if (!functions.count(call->callee))
            throw std::runtime_error("Undefined function: " + call->callee);

        const FunctionDef* func = functions[call->callee];
        if (call->arguments.size() != func->parameters.size())
            throw std::runtime_error("Argument count mismatch in call to " + call->callee);

        // Backup current variables
        auto backup = variables;

        // Bind parameters
        for (size_t i = 0; i < call->arguments.size(); ++i) {
            int argVal = evalExpr(call->arguments[i].get());
            variables[func->parameters[i]] = argVal;
        }

        // Execute function body
        int returnValue = 0;
        for (const auto& stmt : func->body) {
            if (auto ret = dynamic_cast<const Return*>(stmt.get())) {
                returnValue = evalExpr(ret->value.get());
                break;
            } else {
                execute(stmt.get());  // ✅ Proper execution of all valid statements
            }
        }

        // Restore variables
        variables = backup;
        return returnValue;
    }

    throw std::runtime_error("Unknown expression");
}

void execute(const Statement* stmt) {
    if (auto func = dynamic_cast<const FunctionDef*>(stmt)) {
        functions[func->name] = func;  // Just store, do not execute
    } else if (auto assign = dynamic_cast<const Assignment*>(stmt)) {
        int value = evalExpr(assign->value.get());
        variables[assign->name] = value;
    } else if (auto print = dynamic_cast<const Print*>(stmt)) {
        int value = evalExpr(print->value.get());
        std::cout << value << std::endl;
    } else if (auto ifstmt = dynamic_cast<const IfStatement*>(stmt)) {
        int cond = evalExpr(ifstmt->condition.get());
        const auto& branch = cond ? ifstmt->thenBranch : ifstmt->elseBranch;
        for (const auto& subStmt : branch) {
            execute(subStmt.get());
        }
    } else if (auto exprStmt = dynamic_cast<const ExprStatement*>(stmt)) {
        evalExpr(exprStmt->expr.get());  // Handles things like `check(5);`
    } else {
        throw std::runtime_error("Unsupported statement");
    }
}

void run(const std::vector<std::unique_ptr<Statement>>& statements) {
    for (const auto& stmt : statements) {
        execute(stmt.get());  // 🔄 Unified logic here
    }
}
