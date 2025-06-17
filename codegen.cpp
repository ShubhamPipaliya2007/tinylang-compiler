#include "ast.hpp"
#include <iostream>
#include <unordered_map>
#include <fstream>

static std::unordered_map<std::string, int> variables;
static std::unordered_map<std::string, double> float_variables;
static std::unordered_map<std::string, char> char_variables;
static std::unordered_map<std::string, const FunctionDef*> functions;

// Forward declaration
void execute(const Statement* stmt);

enum class ValueType { INT, FLOAT, CHAR };

struct Value {
    ValueType type;
    int i;
    double f;
    char c;
    Value(int v) : type(ValueType::INT), i(v), f(0), c(0) {}
    Value(double v) : type(ValueType::FLOAT), i(0), f(v), c(0) {}
    Value(char v) : type(ValueType::CHAR), i(0), f(0), c(v) {}
};

Value evalExpr(const Expr* expr) {
    if (auto num = dynamic_cast<const Number*>(expr)) {
        return Value(num->value);
    } 
    else if (auto floatLit = dynamic_cast<const FloatLiteral*>(expr)) {
        return Value(floatLit->value);
    }
    else if (auto charLit = dynamic_cast<const CharLiteral*>(expr)) {
        return Value(charLit->value);
    }
    else if (auto boolLit = dynamic_cast<const BoolLiteral*>(expr)) {
        return Value(boolLit->value ? 1 : 0);
    } 
    else if (auto strLit = dynamic_cast<const StringLiteral*>(expr)) {
        throw std::runtime_error("Cannot evaluate string literal to integer");
    } 
    else if (auto var = dynamic_cast<const Variable*>(expr)) {
        if (variables.count(var->name)) return Value(variables[var->name]);
        if (float_variables.count(var->name)) return Value(float_variables[var->name]);
        if (char_variables.count(var->name)) return Value(char_variables[var->name]);
        throw std::runtime_error("Undefined variable: " + var->name);
    } 
    else if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
        Value left = evalExpr(bin->left.get());
        Value right = evalExpr(bin->right.get());
        // Promote to float if either is float
        if (left.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
            double l = (left.type == ValueType::FLOAT) ? left.f : left.i;
            double r = (right.type == ValueType::FLOAT) ? right.f : right.i;
            switch (bin->op) {
                case TokenType::PLUS: return Value(l + r);
                case TokenType::MINUS: return Value(l - r);
                case TokenType::MULTIPLICATION: return Value(l * r);
                case TokenType::DIVISION:
                    if (r == 0) throw std::runtime_error("Division by zero");
                    return Value(l / r);
                case TokenType::GREATERTHEN: return Value(l > r ? 1 : 0);
                case TokenType::LESSTHEN: return Value(l < r ? 1 : 0);
                case TokenType::EQUALTO: return Value(l == r ? 1 : 0);
                case TokenType::NOTEQUALTO: return Value(l != r ? 1 : 0);
                default: throw std::runtime_error("Unsupported binary operator");
            }
        } else if (left.type == ValueType::CHAR && right.type == ValueType::CHAR) {
            char l = left.c, r = right.c;
            switch (bin->op) {
                case TokenType::EQUALTO: return Value(l == r ? 1 : 0);
                case TokenType::NOTEQUALTO: return Value(l != r ? 1 : 0);
                default: throw std::runtime_error("Unsupported binary operator for char");
            }
        } else {
            int l = (left.type == ValueType::INT) ? left.i : left.c;
            int r = (right.type == ValueType::INT) ? right.i : right.c;
            switch (bin->op) {
                case TokenType::PLUS: return Value(l + r);
                case TokenType::MINUS: return Value(l - r);
                case TokenType::MULTIPLICATION: return Value(l * r);
                case TokenType::DIVISION:
                    if (r == 0) throw std::runtime_error("Division by zero");
                    return Value(l / r);
                case TokenType::GREATERTHEN: return Value(l > r ? 1 : 0);
                case TokenType::LESSTHEN: return Value(l < r ? 1 : 0);
                case TokenType::EQUALTO: return Value(l == r ? 1 : 0);
                case TokenType::NOTEQUALTO: return Value(l != r ? 1 : 0);
                default: throw std::runtime_error("Unsupported binary operator");
            }
        }
    } else if (auto input = dynamic_cast<const InputExpr*>(expr)) {
        int val;
        std::cin >> val;
        return Value(val);
    } else if (auto read = dynamic_cast<const ReadExpr*>(expr)) {
        std::ifstream file(read->filename);
        if (!file.is_open())
            throw std::runtime_error("Failed to open file: " + read->filename);
        int val;
        file >> val;
        return Value(val);
    }
    else if (auto call = dynamic_cast<const CallExpr*>(expr)) {
        if (!functions.count(call->callee))
            throw std::runtime_error("Undefined function: " + call->callee);

        const FunctionDef* func = functions[call->callee];
        if (call->arguments.size() != func->parameters.size())
            throw std::runtime_error("Argument count mismatch in call to " + call->callee);

        auto backup = variables;
        auto backupf = float_variables;
        auto backupc = char_variables;

        for (size_t i = 0; i < call->arguments.size(); ++i) {
            Value argVal = evalExpr(call->arguments[i].get());
            // For now, only support int
            variables[func->parameters[i]] = (argVal.type == ValueType::FLOAT) ? (int)argVal.f : (argVal.type == ValueType::CHAR ? argVal.c : argVal.i);
        }

        int returnValue = 0;
        for (const auto& stmt : func->body) {
            if (auto ret = dynamic_cast<const Return*>(stmt.get())) {
                returnValue = evalExpr(ret->value.get()).i;
                break;
            } else {
                execute(stmt.get());
            }
        }

        variables = backup;
        float_variables = backupf;
        char_variables = backupc;
        return Value(returnValue);
    }

    throw std::runtime_error("Unknown expression");
}

void execute(const Statement* stmt) {
    if (auto func = dynamic_cast<const FunctionDef*>(stmt)) {
        functions[func->name] = func;
    } 
    else if (auto assign = dynamic_cast<const Assignment*>(stmt)) {
        Value value = evalExpr(assign->value.get());
        // Assign to correct variable map
        if (dynamic_cast<const FloatLiteral*>(assign->value.get()) || value.type == ValueType::FLOAT) {
            float_variables[assign->name] = (value.type == ValueType::FLOAT) ? value.f : value.i;
        } else if (dynamic_cast<const CharLiteral*>(assign->value.get()) || value.type == ValueType::CHAR) {
            char_variables[assign->name] = (value.type == ValueType::CHAR) ? value.c : value.i;
        } else {
            variables[assign->name] = (value.type == ValueType::INT) ? value.i : (value.type == ValueType::FLOAT ? (int)value.f : value.c);
        }
    } 
    else if (auto print = dynamic_cast<const Print*>(stmt)) {
        if (auto strLit = dynamic_cast<const StringLiteral*>(print->value.get())) {
            std::cout << strLit->value << std::endl;
        } else if (auto boolLit = dynamic_cast<const BoolLiteral*>(print->value.get())) {
            std::cout << (boolLit->value ? "true" : "false") << std::endl;
        } else if (auto floatLit = dynamic_cast<const FloatLiteral*>(print->value.get())) {
            std::cout << floatLit->value << std::endl;
        } else if (auto charLit = dynamic_cast<const CharLiteral*>(print->value.get())) {
            std::cout << charLit->value << std::endl;
        } else if (auto var = dynamic_cast<const Variable*>(print->value.get())) {
            if (variables.count(var->name)) std::cout << variables[var->name] << std::endl;
            else if (float_variables.count(var->name)) std::cout << float_variables[var->name] << std::endl;
            else if (char_variables.count(var->name)) std::cout << char_variables[var->name] << std::endl;
            else throw std::runtime_error("Undefined variable: " + var->name);
        } else {
            Value value = evalExpr(print->value.get());
            if (value.type == ValueType::FLOAT) std::cout << value.f << std::endl;
            else if (value.type == ValueType::CHAR) std::cout << value.c << std::endl;
            else std::cout << value.i << std::endl;
        }
    } 
    else if (auto ifstmt = dynamic_cast<const IfStatement*>(stmt)) {
        int cond = evalExpr(ifstmt->condition.get()).i;
        const auto& branch = cond ? ifstmt->thenBranch : ifstmt->elseBranch;
        for (const auto& subStmt : branch) {
            execute(subStmt.get());
        }
    } 
    else if (auto exprStmt = dynamic_cast<const ExprStatement*>(stmt)) {
        evalExpr(exprStmt->expr.get());
    } 
    else if (auto whilestmt = dynamic_cast<const WhileStatement*>(stmt)) {
        while (evalExpr(whilestmt->condition.get()).i) {
            for (const auto& s : whilestmt->body) {
                execute(s.get());
            }
        }
    } 
    else if (auto forstmt = dynamic_cast<const ForStatement*>(stmt)) {
        if (forstmt->initializer) execute(forstmt->initializer.get());
        while (!forstmt->condition || evalExpr(forstmt->condition.get()).i) {
            for (const auto& s : forstmt->body) {
                execute(s.get());
            }
            if (forstmt->increment) execute(forstmt->increment.get());
        }
    } 
    else {
        throw std::runtime_error("Unsupported statement");
    }
}

void run(const std::vector<std::unique_ptr<Statement>>& statements) {
    for (const auto& stmt : statements) {
        execute(stmt.get());
    }
}
