#include "ast.hpp"
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <string>

static std::vector<std::unordered_map<std::string, int>> variables_stack{ { } };
static std::vector<std::unordered_map<std::string, double>> float_variables_stack{ { } };
static std::vector<std::unordered_map<std::string, char>> char_variables_stack{ { } };
static std::vector<std::unordered_map<std::string, std::string>> string_variables_stack{ { } };
static std::unordered_map<std::string, std::vector<int>> int_arrays;
static std::unordered_map<std::string, std::vector<double>> float_arrays;
static std::unordered_map<std::string, std::vector<char>> char_arrays;
static std::unordered_map<std::string, std::vector<bool>> bool_arrays;
static std::unordered_map<std::string, std::vector<std::string>> string_arrays;
static std::unordered_map<std::string, const FunctionDef*> functions;

// Forward declaration
void execute(const Statement* stmt);

enum class ValueType { INT, FLOAT, CHAR, STRING };

struct Value {
    ValueType type;
    int i;
    double f;
    char c;
    std::string s;
    Value(int v) : type(ValueType::INT), i(v), f(0), c(0) {}
    Value(double v) : type(ValueType::FLOAT), i(0), f(v), c(0) {}
    Value(char v) : type(ValueType::CHAR), i(0), f(0), c(v) {}
    Value(const std::string& str) : type(ValueType::STRING), i(0), f(0), c(0), s(str) {}
};

// Helper functions for variable lookup and assignment
int getIntVar(const std::string& name) {
    for (auto it = variables_stack.rbegin(); it != variables_stack.rend(); ++it) {
        if (it->count(name)) return (*it)[name];
    }
    throw std::runtime_error("Undefined variable: " + name);
}
void setIntVar(const std::string& name, int value) {
    // Only check the current scope (top of stack)
    if (variables_stack.back().count(name)) {
        variables_stack.back()[name] = value;
    } else {
        variables_stack.back()[name] = value;
    }
}
double getFloatVar(const std::string& name) {
    for (auto it = float_variables_stack.rbegin(); it != float_variables_stack.rend(); ++it) {
        if (it->count(name)) return (*it)[name];
    }
    throw std::runtime_error("Undefined float variable: " + name);
}
void setFloatVar(const std::string& name, double value) {
    for (auto it = float_variables_stack.rbegin(); it != float_variables_stack.rend(); ++it) {
        if (it->count(name)) { (*it)[name] = value; return; }
    }
    float_variables_stack.back()[name] = value;
}
char getCharVar(const std::string& name) {
    for (auto it = char_variables_stack.rbegin(); it != char_variables_stack.rend(); ++it) {
        if (it->count(name)) return (*it)[name];
    }
    throw std::runtime_error("Undefined char variable: " + name);
}
void setCharVar(const std::string& name, char value) {
    for (auto it = char_variables_stack.rbegin(); it != char_variables_stack.rend(); ++it) {
        if (it->count(name)) { (*it)[name] = value; return; }
    }
    char_variables_stack.back()[name] = value;
}
std::string getStringVar(const std::string& name) {
    for (auto it = string_variables_stack.rbegin(); it != string_variables_stack.rend(); ++it) {
        if (it->count(name)) return (*it)[name];
    }
    throw std::runtime_error("Undefined string variable: " + name);
}
void setStringVar(const std::string& name, const std::string& value) {
    for (auto it = string_variables_stack.rbegin(); it != string_variables_stack.rend(); ++it) {
        if (it->count(name)) { (*it)[name] = value; return; }
    }
    string_variables_stack.back()[name] = value;
}

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
        return Value(strLit->value);
    } 
    else if (auto var = dynamic_cast<const Variable*>(expr)) {
        try { return Value(getIntVar(var->name)); } catch (...) {}
        try { return Value(getFloatVar(var->name)); } catch (...) {}
        try { return Value(getCharVar(var->name)); } catch (...) {}
        try { return Value(getStringVar(var->name)); } catch (...) {}
        throw std::runtime_error("Undefined variable: " + var->name);
    } 
    else if (auto unary = dynamic_cast<const UnaryExpr*>(expr)) {
        Value operand = evalExpr(unary->operand.get());
        if (unary->op == TokenType::NOT) {
            return Value(operand.i == 0 ? 1 : 0);
        }
        throw std::runtime_error("Unsupported unary operator");
    }
    else if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
        // Logical operators with short-circuit evaluation
        if (bin->op == TokenType::AND) {
            Value left = evalExpr(bin->left.get());
            if (left.i == 0) return Value(0); // Short-circuit: false && anything = false
            Value right = evalExpr(bin->right.get());
            return Value((left.i != 0 && right.i != 0) ? 1 : 0);
        }
        if (bin->op == TokenType::OR) {
            Value left = evalExpr(bin->left.get());
            if (left.i != 0) return Value(1); // Short-circuit: true || anything = true
            Value right = evalExpr(bin->right.get());
            return Value((left.i != 0 || right.i != 0) ? 1 : 0);
        }
        // Evaluate both sides first
        Value left = evalExpr(bin->left.get());
        Value right = evalExpr(bin->right.get());
        // String concatenation if either is a string
        if (bin->op == TokenType::PLUS) {
            if (left.type == ValueType::STRING || right.type == ValueType::STRING) {
                std::string lstr = (left.type == ValueType::STRING) ? left.s : std::to_string(left.i);
                std::string rstr = (right.type == ValueType::STRING) ? right.s : std::to_string(right.i);
                return Value(lstr + rstr);
            }
        }
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
        // Push new local scopes
        variables_stack.push_back({});
        float_variables_stack.push_back({});
        char_variables_stack.push_back({});
        string_variables_stack.push_back({});
        for (size_t i = 0; i < call->arguments.size(); ++i) {
            Value argVal = evalExpr(call->arguments[i].get());
            if (argVal.type == ValueType::FLOAT) setFloatVar(func->parameters[i], argVal.f);
            else if (argVal.type == ValueType::CHAR) setCharVar(func->parameters[i], argVal.c);
            else if (argVal.type == ValueType::STRING) setStringVar(func->parameters[i], argVal.s);
            else setIntVar(func->parameters[i], argVal.i);
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

        // Pop local scopes
        if (variables_stack.size() > 1) variables_stack.pop_back();
        if (float_variables_stack.size() > 1) float_variables_stack.pop_back();
        if (char_variables_stack.size() > 1) char_variables_stack.pop_back();
        if (string_variables_stack.size() > 1) string_variables_stack.pop_back();
        return Value(returnValue);
    }
    else if (auto arrAccess = dynamic_cast<const ArrayAccess*>(expr)) {
        const std::string& name = arrAccess->arrayName;
        int idx = evalExpr(arrAccess->index.get()).i;
        if (int_arrays.count(name)) return Value(int_arrays[name].at(idx));
        if (float_arrays.count(name)) return Value(float_arrays[name].at(idx));
        if (char_arrays.count(name)) return Value(char_arrays[name].at(idx));
        if (bool_arrays.count(name)) return Value(bool_arrays[name].at(idx));
        if (string_arrays.count(name)) throw std::runtime_error("Cannot use string array element as int/float/char");
        throw std::runtime_error("Undefined array: " + name);
    }
    else if (auto arrLit = dynamic_cast<const ArrayLiteral*>(expr)) {
        // Only used for initialization, handled in execute
        throw std::runtime_error("ArrayLiteral should not be evaluated directly");
    }

    throw std::runtime_error("Unknown expression");
}

void execute(const Statement* stmt) {
    if (auto func = dynamic_cast<const FunctionDef*>(stmt)) {
        functions[func->name] = func;
    } 
    else if (auto assign = dynamic_cast<const Assignment*>(stmt)) {
        // Handle normal variable declarations with initialization
        if (!assign->type.empty() && assign->value) {
            if (assign->type == "int" && dynamic_cast<const Number*>(assign->value.get())) {
                setIntVar(assign->name, evalExpr(assign->value.get()).i);
                return;
            } else if (assign->type == "float" && dynamic_cast<const FloatLiteral*>(assign->value.get())) {
                setFloatVar(assign->name, evalExpr(assign->value.get()).f);
                return;
            } else if (assign->type == "char" && dynamic_cast<const CharLiteral*>(assign->value.get())) {
                setCharVar(assign->name, evalExpr(assign->value.get()).c);
                return;
            } else if (assign->type == "bool" && dynamic_cast<const BoolLiteral*>(assign->value.get())) {
                setIntVar(assign->name, evalExpr(assign->value.get()).i);
                return;
            } else if (assign->type == "string" && dynamic_cast<const StringLiteral*>(assign->value.get())) {
                setStringVar(assign->name, evalExpr(assign->value.get()).s);
                return;
            }
        }
        // Fixed-size array declaration: e.g., float nums[3];
        if (!assign->type.empty() && assign->value && dynamic_cast<const Number*>(assign->value.get()) && assign->name.find('[') != std::string::npos) {
            int size = evalExpr(assign->value.get()).i;
            if (assign->type == "float") {
                float_arrays[assign->name] = std::vector<double>(size, 0.0);
            } else if (assign->type == "int") {
                int_arrays[assign->name] = std::vector<int>(size, 0);
            } else if (assign->type == "char") {
                char_arrays[assign->name] = std::vector<char>(size, '\0');
            } else if (assign->type == "bool") {
                bool_arrays[assign->name] = std::vector<bool>(size, false);
            } else if (assign->type == "string") {
                string_arrays[assign->name] = std::vector<std::string>(size, "");
            } else {
                int_arrays[assign->name] = std::vector<int>(size, 0);
            }
        }
        // Array declaration/initialization
        else if (auto arrLit = dynamic_cast<const ArrayLiteral*>(assign->value.get())) {
            // Try to infer type from first element
            if (arrLit->elements.empty()) {
                int_arrays[assign->name] = {};
            } else if (dynamic_cast<const Number*>(arrLit->elements[0].get())) {
                std::vector<int> vals;
                for (const auto& e : arrLit->elements) vals.push_back(evalExpr(e.get()).i);
                int_arrays[assign->name] = vals;
            } else if (dynamic_cast<const FloatLiteral*>(arrLit->elements[0].get())) {
                std::vector<double> vals;
                for (const auto& e : arrLit->elements) vals.push_back(evalExpr(e.get()).f);
                float_arrays[assign->name] = vals;
            } else if (dynamic_cast<const CharLiteral*>(arrLit->elements[0].get())) {
                std::vector<char> vals;
                for (const auto& e : arrLit->elements) vals.push_back(evalExpr(e.get()).c);
                char_arrays[assign->name] = vals;
            } else if (dynamic_cast<const BoolLiteral*>(arrLit->elements[0].get())) {
                std::vector<bool> vals;
                for (const auto& e : arrLit->elements) vals.push_back(evalExpr(e.get()).i != 0);
                bool_arrays[assign->name] = vals;
            } else if (dynamic_cast<const StringLiteral*>(arrLit->elements[0].get())) {
                std::vector<std::string> vals;
                for (const auto& e : arrLit->elements) vals.push_back(dynamic_cast<const StringLiteral*>(e.get())->value);
                string_arrays[assign->name] = vals;
            } else {
                throw std::runtime_error("Unsupported array literal type");
            }
        } else if (assign->value && dynamic_cast<const FloatLiteral*>(assign->value.get())) {
            setFloatVar(assign->name, evalExpr(assign->value.get()).f);
        } else if (assign->value && dynamic_cast<const CharLiteral*>(assign->value.get())) {
            setCharVar(assign->name, evalExpr(assign->value.get()).c);
        } else if (assign->value && dynamic_cast<const BoolLiteral*>(assign->value.get())) {
            setIntVar(assign->name, evalExpr(assign->value.get()).i);
        } else if (assign->value && dynamic_cast<const StringLiteral*>(assign->value.get())) {
            setStringVar(assign->name, dynamic_cast<const StringLiteral*>(assign->value.get())->value);
        } else if (assign->value && dynamic_cast<const Variable*>(assign->value.get())) {
            auto var = dynamic_cast<const Variable*>(assign->value.get());
            if (string_variables_stack.back().count(var->name)) {
                setStringVar(assign->name, string_variables_stack.back()[var->name]);
            } else {
                setIntVar(assign->name, evalExpr(assign->value.get()).i);
            }
        } else if (assign->value) {
            Value val = evalExpr(assign->value.get());
            if (val.type == ValueType::STRING) setStringVar(assign->name, val.s);
            else if (val.type == ValueType::FLOAT) setFloatVar(assign->name, val.f);
            else if (val.type == ValueType::CHAR) setCharVar(assign->name, val.c);
            else setIntVar(assign->name, val.i);
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
            try { std::cout << getIntVar(var->name) << std::endl; return; } catch (...) {}
            try { std::cout << getFloatVar(var->name) << std::endl; return; } catch (...) {}
            try { std::cout << getCharVar(var->name) << std::endl; return; } catch (...) {}
            try { std::cout << getStringVar(var->name) << std::endl; return; } catch (...) {}
            throw std::runtime_error("Undefined variable: " + var->name);
        } else if (auto arrAccess = dynamic_cast<const ArrayAccess*>(print->value.get())) {
            const std::string& name = arrAccess->arrayName;
            int idx = evalExpr(arrAccess->index.get()).i;
            if (int_arrays.count(name)) std::cout << int_arrays[name].at(idx) << std::endl;
            else if (float_arrays.count(name)) std::cout << float_arrays[name].at(idx) << std::endl;
            else if (char_arrays.count(name)) std::cout << char_arrays[name].at(idx) << std::endl;
            else if (bool_arrays.count(name)) std::cout << (bool_arrays[name].at(idx) ? "true" : "false") << std::endl;
            else if (string_arrays.count(name)) std::cout << string_arrays[name].at(idx) << std::endl;
            else throw std::runtime_error("Undefined array: " + name);
        } else {
            Value value = evalExpr(print->value.get());
            if (value.type == ValueType::STRING) std::cout << value.s << std::endl;
            else if (value.type == ValueType::FLOAT) std::cout << value.f << std::endl;
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
    else if (auto arrAssign = dynamic_cast<const ArrayAssignment*>(stmt)) {
        const std::string& name = arrAssign->arrayName;
        int idx = evalExpr(arrAssign->index.get()).i;
        Value value = evalExpr(arrAssign->value.get());
        if (int_arrays.count(name)) int_arrays[name].at(idx) = value.i;
        else if (float_arrays.count(name)) float_arrays[name].at(idx) = (value.type == ValueType::FLOAT ? value.f : value.i);
        else if (char_arrays.count(name)) char_arrays[name].at(idx) = (value.type == ValueType::CHAR ? value.c : value.i);
        else if (bool_arrays.count(name)) bool_arrays[name].at(idx) = (value.i != 0);
        else throw std::runtime_error("Undefined array: " + name);
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
