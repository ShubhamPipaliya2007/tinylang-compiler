#include "ast.hpp"
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <vector>
#include <string>
#include <variant>
#include <algorithm>

std::unordered_map<std::string, const ClassDef*> class_defs;
// Helper: Recursively collect all fields from base classes
static void collectFields(const ClassDef* classDef, std::vector<std::pair<std::string, std::string>>& outFields) {
    if (!classDef->baseClass.empty()) {
        if (!class_defs.count(classDef->baseClass)) {
            throw std::runtime_error("Base class not found: " + classDef->baseClass);
        }
        collectFields(class_defs[classDef->baseClass], outFields);
    }
    // Add/override fields from this class
    for (const auto& field : classDef->fields) {
        // If already present, override
        auto it = std::find_if(outFields.begin(), outFields.end(), [&](const auto& f) { return f.second == field.second; });
        if (it != outFields.end()) *it = field;
        else outFields.push_back(field);
    }
}

// Helper: Recursively collect all methods from base classes
static void collectMethods(const ClassDef* classDef, std::vector<const FunctionDef*>& outMethods) {
    if (!classDef->baseClass.empty()) {
        if (!class_defs.count(classDef->baseClass)) {
            throw std::runtime_error("Base class not found: " + classDef->baseClass);
        }
        collectMethods(class_defs[classDef->baseClass], outMethods);
    }
    // Add/override methods from this class
    for (const auto& method : classDef->methods) {
        auto it = std::find_if(outMethods.begin(), outMethods.end(), [&](const FunctionDef* m) { return m->name == method->name; });
        if (it != outMethods.end()) *it = method.get();
        else outMethods.push_back(method.get());
    }
}

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

// Object model for class instances
struct ObjectInstance {
    std::string className;
    std::unordered_map<std::string, std::variant<int, double, char, std::string>> fields;
};

static std::unordered_map<std::string, ObjectInstance> objects;

// Add storage for object arrays
static std::unordered_map<std::string, std::vector<ObjectInstance>> object_arrays;

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
        // Object array access
        if (object_arrays.count(arrAccess->arrayName)) {
            int idx = evalExpr(arrAccess->index.get()).i;
            if (idx < 0 || idx >= (int)object_arrays[arrAccess->arrayName].size())
                throw std::runtime_error("Object array index out of bounds: " + std::to_string(idx));
            // Return a proxy Variable for further member access
            // We'll use a special Variable name: arrayName[idx]
            return Value(arrAccess->arrayName + "[" + std::to_string(idx) + "]");
        }
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
    else if (auto objAccess = dynamic_cast<const ObjectMemberAccess*>(expr)) {
        // Support member access on object arrays
        if (auto var = dynamic_cast<const Variable*>(objAccess->object.get())) {
            std::string objName = var->name;
            // Check for object array element proxy
            size_t lb = objName.find('[');
            size_t rb = objName.find(']');
            if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
                std::string arrName = objName.substr(0, lb);
                int idx = std::stoi(objName.substr(lb+1, rb-lb-1));
                if (!object_arrays.count(arrName)) throw std::runtime_error("Undefined object array: " + arrName);
                const ObjectInstance& inst = object_arrays[arrName].at(idx);
                if (inst.fields.count(objAccess->member)) {
                    const auto& val = inst.fields.at(objAccess->member);
                    if (std::holds_alternative<int>(val)) return Value(std::get<int>(val));
                    if (std::holds_alternative<double>(val)) return Value(std::get<double>(val));
                    if (std::holds_alternative<char>(val)) return Value(std::get<char>(val));
                    if (std::holds_alternative<std::string>(val)) return Value(std::get<std::string>(val));
                    throw std::runtime_error("Unsupported field type");
                }
                throw std::runtime_error("Field not found in object array element");
            }
            // Evaluate the object expression to get the variable name
            if (!objects.count(objName)) throw std::runtime_error("Undefined object: " + objName);
            const ObjectInstance& inst = objects.at(objName);
            // Field access
            if (inst.fields.count(objAccess->member)) {
                const auto& val = inst.fields.at(objAccess->member);
                if (std::holds_alternative<int>(val)) return Value(std::get<int>(val));
                if (std::holds_alternative<double>(val)) return Value(std::get<double>(val));
                if (std::holds_alternative<char>(val)) return Value(std::get<char>(val));
                if (std::holds_alternative<std::string>(val)) return Value(std::get<std::string>(val));
                throw std::runtime_error("Unsupported field type");
            }
            // Method access (not yet implemented)
            throw std::runtime_error("Method calls on objects not yet implemented");
        }
        throw std::runtime_error("Unsupported object member access");
    }
    else if (auto objMethod = dynamic_cast<const ObjectMethodCall*>(expr)) {
        // Support method call on object array element
        if (auto var = dynamic_cast<const Variable*>(objMethod->object.get())) {
            std::string objName = var->name;
            size_t lb = objName.find('[');
            size_t rb = objName.find(']');
            if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
                std::string arrName = objName.substr(0, lb);
                int idx = std::stoi(objName.substr(lb+1, rb-lb-1));
                if (!object_arrays.count(arrName)) throw std::runtime_error("Undefined object array: " + arrName);
                ObjectInstance& inst = object_arrays[arrName].at(idx);
                // Find the class definition
                if (!class_defs.count(inst.className)) throw std::runtime_error("Class not found: " + inst.className);
                const ClassDef* classDef = class_defs.at(inst.className);
                // Find the method
                const FunctionDef* method = nullptr;
                std::vector<const FunctionDef*> allMethods;
                collectMethods(classDef, allMethods);
                for (const auto& m : allMethods) {
                    if (m->name == objMethod->method) {
                        method = m;
                        break;
                    }
                }
                if (!method) throw std::runtime_error("Method not found: " + objMethod->method + " in class " + inst.className);
                if (objMethod->arguments.size() != method->parameters.size())
                    throw std::runtime_error("Argument count mismatch in call to method " + objMethod->method);
                // Push new local scopes
                variables_stack.push_back({});
                float_variables_stack.push_back({});
                char_variables_stack.push_back({});
                string_variables_stack.push_back({});
                // Set up fields as locals
                for (const auto& field : inst.fields) {
                    if (std::holds_alternative<int>(field.second)) setIntVar(field.first, std::get<int>(field.second));
                    else if (std::holds_alternative<double>(field.second)) setFloatVar(field.first, std::get<double>(field.second));
                    else if (std::holds_alternative<char>(field.second)) setCharVar(field.first, std::get<char>(field.second));
                    else if (std::holds_alternative<std::string>(field.second)) setStringVar(field.first, std::get<std::string>(field.second));
                }
                // Set up parameters
                for (size_t i = 0; i < objMethod->arguments.size(); ++i) {
                    Value argVal = evalExpr(objMethod->arguments[i].get());
                    if (argVal.type == ValueType::FLOAT) setFloatVar(method->parameters[i], argVal.f);
                    else if (argVal.type == ValueType::CHAR) setCharVar(method->parameters[i], argVal.c);
                    else if (argVal.type == ValueType::STRING) setStringVar(method->parameters[i], argVal.s);
                    else setIntVar(method->parameters[i], argVal.i);
                }
                int returnValue = 0;
                for (const auto& stmt : method->body) {
                    if (auto ret = dynamic_cast<const Return*>(stmt.get())) {
                        returnValue = evalExpr(ret->value.get()).i;
                        break;
                    } else {
                        execute(stmt.get());
                    }
                }
                // Update fields from local scope after method call
                for (auto& field : class_defs[inst.className]->fields) {
                    const std::string& fname = field.second;
                    if (string_variables_stack.back().count(fname)) inst.fields[fname] = string_variables_stack.back()[fname];
                    else if (variables_stack.back().count(fname)) inst.fields[fname] = variables_stack.back()[fname];
                    else if (float_variables_stack.back().count(fname)) inst.fields[fname] = float_variables_stack.back()[fname];
                    else if (char_variables_stack.back().count(fname)) inst.fields[fname] = char_variables_stack.back()[fname];
                }
                // Pop local scopes
                if (variables_stack.size() > 1) variables_stack.pop_back();
                if (float_variables_stack.size() > 1) float_variables_stack.pop_back();
                if (char_variables_stack.size() > 1) char_variables_stack.pop_back();
                if (string_variables_stack.size() > 1) string_variables_stack.pop_back();
                return Value(returnValue);
            } else {
                // Single object instance
                if (!objects.count(objName)) throw std::runtime_error("Undefined object: " + objName);
                ObjectInstance& inst = objects[objName];
                // Find the class definition
                if (!class_defs.count(inst.className)) throw std::runtime_error("Class not found: " + inst.className);
                const ClassDef* classDef = class_defs.at(inst.className);
                // Find the method (search inheritance chain)
                const FunctionDef* method = nullptr;
                std::vector<const FunctionDef*> allMethods;
                collectMethods(classDef, allMethods);
                for (const auto& m : allMethods) {
                    if (m->name == objMethod->method) {
                        method = m;
                        break;
                    }
                }
                if (!method) throw std::runtime_error("Method not found: " + objMethod->method + " in class " + inst.className);
                if (objMethod->arguments.size() != method->parameters.size())
                    throw std::runtime_error("Argument count mismatch in call to method " + objMethod->method);
                // Push new local scopes
                variables_stack.push_back({});
                float_variables_stack.push_back({});
                char_variables_stack.push_back({});
                string_variables_stack.push_back({});
                // Set up fields as locals
                for (const auto& field : inst.fields) {
                    if (std::holds_alternative<int>(field.second)) setIntVar(field.first, std::get<int>(field.second));
                    else if (std::holds_alternative<double>(field.second)) setFloatVar(field.first, std::get<double>(field.second));
                    else if (std::holds_alternative<char>(field.second)) setCharVar(field.first, std::get<char>(field.second));
                    else if (std::holds_alternative<std::string>(field.second)) setStringVar(field.first, std::get<std::string>(field.second));
                }
                // Set up parameters
                for (size_t i = 0; i < objMethod->arguments.size(); ++i) {
                    Value argVal = evalExpr(objMethod->arguments[i].get());
                    if (argVal.type == ValueType::FLOAT) setFloatVar(method->parameters[i], argVal.f);
                    else if (argVal.type == ValueType::CHAR) setCharVar(method->parameters[i], argVal.c);
                    else if (argVal.type == ValueType::STRING) setStringVar(method->parameters[i], argVal.s);
                    else setIntVar(method->parameters[i], argVal.i);
                }
                int returnValue = 0;
                for (const auto& stmt : method->body) {
                    if (auto ret = dynamic_cast<const Return*>(stmt.get())) {
                        returnValue = evalExpr(ret->value.get()).i;
                        break;
                    } else {
                        execute(stmt.get());
                    }
                }
                // Update fields from local scope after method call
                for (auto& field : inst.fields) {
                    const std::string& fname = field.first;
                    if (string_variables_stack.back().count(fname)) field.second = string_variables_stack.back()[fname];
                    else if (variables_stack.back().count(fname)) field.second = variables_stack.back()[fname];
                    else if (float_variables_stack.back().count(fname)) field.second = float_variables_stack.back()[fname];
                    else if (char_variables_stack.back().count(fname)) field.second = char_variables_stack.back()[fname];
                }
                // Pop local scopes
                if (variables_stack.size() > 1) variables_stack.pop_back();
                if (float_variables_stack.size() > 1) float_variables_stack.pop_back();
                if (char_variables_stack.size() > 1) char_variables_stack.pop_back();
                if (string_variables_stack.size() > 1) string_variables_stack.pop_back();
                return Value(returnValue);
            }
        } else if (auto arr = dynamic_cast<const ArrayAccess*>(objMethod->object.get())) {
            // Handle p[0].greet() where object is ArrayAccess
            std::string arrName = arr->arrayName;
            Value idxVal = evalExpr(arr->index.get());
            int idx = idxVal.i;
            if (!object_arrays.count(arrName)) throw std::runtime_error("Undefined object array: " + arrName);
            ObjectInstance& inst = object_arrays[arrName].at(idx);
            // Find the class definition
            if (!class_defs.count(inst.className)) throw std::runtime_error("Class not found: " + inst.className);
            const ClassDef* classDef = class_defs.at(inst.className);
            // Find the method
            const FunctionDef* method = nullptr;
            std::vector<const FunctionDef*> allMethods;
            collectMethods(classDef, allMethods);
            for (const auto& m : allMethods) {
                if (m->name == objMethod->method) {
                    method = m;
                    break;
                }
            }
            if (!method) throw std::runtime_error("Method not found: " + objMethod->method + " in class " + inst.className);
            if (objMethod->arguments.size() != method->parameters.size())
                throw std::runtime_error("Argument count mismatch in call to method " + objMethod->method);
            // Push new local scopes
            variables_stack.push_back({});
            float_variables_stack.push_back({});
            char_variables_stack.push_back({});
            string_variables_stack.push_back({});
            // Set up fields as locals
            for (const auto& field : inst.fields) {
                if (std::holds_alternative<int>(field.second)) setIntVar(field.first, std::get<int>(field.second));
                else if (std::holds_alternative<double>(field.second)) setFloatVar(field.first, std::get<double>(field.second));
                else if (std::holds_alternative<char>(field.second)) setCharVar(field.first, std::get<char>(field.second));
                else if (std::holds_alternative<std::string>(field.second)) setStringVar(field.first, std::get<std::string>(field.second));
            }
            // Set up parameters
            for (size_t i = 0; i < objMethod->arguments.size(); ++i) {
                Value argVal = evalExpr(objMethod->arguments[i].get());
                if (argVal.type == ValueType::FLOAT) setFloatVar(method->parameters[i], argVal.f);
                else if (argVal.type == ValueType::CHAR) setCharVar(method->parameters[i], argVal.c);
                else if (argVal.type == ValueType::STRING) setStringVar(method->parameters[i], argVal.s);
                else setIntVar(method->parameters[i], argVal.i);
            }
            int returnValue = 0;
            for (const auto& stmt : method->body) {
                if (auto ret = dynamic_cast<const Return*>(stmt.get())) {
                    returnValue = evalExpr(ret->value.get()).i;
                    break;
                } else {
                    execute(stmt.get());
                }
            }
            // Update fields from local scope after method call
            for (auto& field : class_defs[inst.className]->fields) {
                const std::string& fname = field.second;
                if (string_variables_stack.back().count(fname)) inst.fields[fname] = string_variables_stack.back()[fname];
                else if (variables_stack.back().count(fname)) inst.fields[fname] = variables_stack.back()[fname];
                else if (float_variables_stack.back().count(fname)) inst.fields[fname] = float_variables_stack.back()[fname];
                else if (char_variables_stack.back().count(fname)) inst.fields[fname] = char_variables_stack.back()[fname];
            }
            // Pop local scopes
            if (variables_stack.size() > 1) variables_stack.pop_back();
            if (float_variables_stack.size() > 1) float_variables_stack.pop_back();
            if (char_variables_stack.size() > 1) char_variables_stack.pop_back();
            if (string_variables_stack.size() > 1) string_variables_stack.pop_back();
            return Value(returnValue);
        }
        throw std::runtime_error("Unsupported object method call");
    }

    throw std::runtime_error("Unknown expression");
}

void execute(const Statement* stmt) {
    // Function definitions
    if (auto func = dynamic_cast<const FunctionDef*>(stmt)) {
        functions[func->name] = func;
    }

    // Assignment handling (includes object instantiation, field assignment, variables, etc.)
    else if (auto assign = dynamic_cast<const Assignment*>(stmt)) {
        // Object array declaration: type ends with []
        if (!assign->type.empty() && assign->type.size() > 2 && assign->type.substr(assign->type.size()-2) == "[]" && assign->value) {
            std::string className = assign->type.substr(0, assign->type.size()-2);
            int size = evalExpr(assign->value.get()).i;
            std::vector<ObjectInstance> arr(size);
            for (int i = 0; i < size; ++i) {
                arr[i].className = className;
                for (const auto& field : class_defs[className]->fields) {
                    if (field.first == "int") arr[i].fields[field.second] = 0;
                    else if (field.first == "float") arr[i].fields[field.second] = 0.0;
                    else if (field.first == "char") arr[i].fields[field.second] = '\0';
                    else if (field.first == "string") arr[i].fields[field.second] = std::string("");
                    else if (field.first == "bool") arr[i].fields[field.second] = 0;
                }
            }
            object_arrays[assign->name] = arr;
            return;
        }

        // 🔁 OBJECT INSTANTIATION FIRST
        if (class_defs.count(assign->type) && assign->value == nullptr) {
            // Create the object instance
            ObjectInstance inst;
            inst.className = assign->type;
            for (const auto& field : class_defs[assign->type]->fields) {
                if (field.first == "int") inst.fields[field.second] = 0;
                else if (field.first == "float") inst.fields[field.second] = 0.0;
                else if (field.first == "char") inst.fields[field.second] = '\0';
                else if (field.first == "string") inst.fields[field.second] = std::string("");
                else if (field.first == "bool") inst.fields[field.second] = 0;
            }
            objects[assign->name] = inst;
            return;
        }

        // 📌 FIELD ASSIGNMENT (like p.name = "Shubham" or p[0].name = ...)
        if (assign->name.find('.') != std::string::npos) {
            size_t dot = assign->name.find('.');
            std::string objName = assign->name.substr(0, dot);
            std::string fieldName = assign->name.substr(dot + 1);

            // Check for object array element (e.g., p[0].name)
            size_t lb = objName.find('[');
            size_t rb = objName.find(']');
            if (lb != std::string::npos && rb != std::string::npos && rb > lb) {
                std::string arrName = objName.substr(0, lb);
                int idx = std::stoi(objName.substr(lb+1, rb-lb-1));
                if (!object_arrays.count(arrName)) throw std::runtime_error("Undefined object array: " + arrName);
                if (idx < 0 || idx >= (int)object_arrays[arrName].size()) throw std::runtime_error("Object array index out of bounds: " + std::to_string(idx));
                Value val = evalExpr(assign->value.get());
                ObjectInstance& inst = object_arrays[arrName][idx];
                inst.fields[fieldName] =
                    (val.type == ValueType::INT) ? std::variant<int, double, char, std::string>(val.i) :
                    (val.type == ValueType::FLOAT) ? std::variant<int, double, char, std::string>(val.f) :
                    (val.type == ValueType::CHAR) ? std::variant<int, double, char, std::string>(val.c) :
                    std::variant<int, double, char, std::string>(val.s);
                return;
            }

            if (!objects.count(objName)) throw std::runtime_error("Undefined object: " + objName);
            Value val = evalExpr(assign->value.get());

            objects[objName].fields[fieldName] =
                (val.type == ValueType::INT) ? std::variant<int, double, char, std::string>(val.i) :
                (val.type == ValueType::FLOAT) ? std::variant<int, double, char, std::string>(val.f) :
                (val.type == ValueType::CHAR) ? std::variant<int, double, char, std::string>(val.c) :
                std::variant<int, double, char, std::string>(val.s);
            return;
        }

        // 📌 Primitive variable declarations with literals
        if (!assign->type.empty() && assign->value) {
            if (assign->type == "int" && dynamic_cast<const Number*>(assign->value.get())) {
                setIntVar(assign->name, evalExpr(assign->value.get()).i); return;
            } else if (assign->type == "float" && dynamic_cast<const FloatLiteral*>(assign->value.get())) {
                setFloatVar(assign->name, evalExpr(assign->value.get()).f); return;
            } else if (assign->type == "char" && dynamic_cast<const CharLiteral*>(assign->value.get())) {
                setCharVar(assign->name, evalExpr(assign->value.get()).c); return;
            } else if (assign->type == "bool" && dynamic_cast<const BoolLiteral*>(assign->value.get())) {
                setIntVar(assign->name, evalExpr(assign->value.get()).i); return;
            } else if (assign->type == "string" && dynamic_cast<const StringLiteral*>(assign->value.get())) {
                setStringVar(assign->name, evalExpr(assign->value.get()).s); return;
            }
        }

        // 📌 Arrays (omit here for brevity — yours already handles this correctly)

        // 📌 Fallback: evaluate and assign
        if (assign->value) {
            Value val = evalExpr(assign->value.get());
            if (val.type == ValueType::STRING) setStringVar(assign->name, val.s);
            else if (val.type == ValueType::FLOAT) setFloatVar(assign->name, val.f);
            else if (val.type == ValueType::CHAR) setCharVar(assign->name, val.c);
            else setIntVar(assign->name, val.i);
        }
    }

    // Print statement
    else if (auto print = dynamic_cast<const Print*>(stmt)) {
        Value value = evalExpr(print->value.get());
        if (value.type == ValueType::STRING) std::cout << value.s << std::endl;
        else if (value.type == ValueType::FLOAT) std::cout << value.f << std::endl;
        else if (value.type == ValueType::CHAR) std::cout << value.c << std::endl;
        else std::cout << value.i << std::endl;
        return;
    }

    // If-statement
    else if (auto ifstmt = dynamic_cast<const IfStatement*>(stmt)) {
        int cond = evalExpr(ifstmt->condition.get()).i;
        const auto& branch = cond ? ifstmt->thenBranch : ifstmt->elseBranch;
        for (const auto& s : branch) execute(s.get());
    }

    // Expression statement
    else if (auto exprStmt = dynamic_cast<const ExprStatement*>(stmt)) {
        // If the expression is an ObjectMethodCall, evaluate it for side effects
        if (dynamic_cast<const ObjectMethodCall*>(exprStmt->expr.get())) {
            evalExpr(exprStmt->expr.get());
            return;
        }
        // Otherwise, just evaluate the expression
        evalExpr(exprStmt->expr.get());
    }

    // While loop
    else if (auto whilestmt = dynamic_cast<const WhileStatement*>(stmt)) {
        while (evalExpr(whilestmt->condition.get()).i) {
            for (const auto& s : whilestmt->body) execute(s.get());
        }
    }

    // For loop
    else if (auto forstmt = dynamic_cast<const ForStatement*>(stmt)) {
        if (forstmt->initializer) execute(forstmt->initializer.get());
        while (!forstmt->condition || evalExpr(forstmt->condition.get()).i) {
            for (const auto& s : forstmt->body) execute(s.get());
            if (forstmt->increment) execute(forstmt->increment.get());
        }
    }

    // Array assignment
    else if (auto arrAssign = dynamic_cast<const ArrayAssignment*>(stmt)) {
        const std::string& name = arrAssign->arrayName;
        int idx = evalExpr(arrAssign->index.get()).i;
        Value value = evalExpr(arrAssign->value.get());

        if (int_arrays.count(name)) int_arrays[name][idx] = value.i;
        else if (float_arrays.count(name)) float_arrays[name][idx] = (value.type == ValueType::FLOAT ? value.f : value.i);
        else if (char_arrays.count(name)) char_arrays[name][idx] = (value.type == ValueType::CHAR ? value.c : value.i);
        else if (bool_arrays.count(name)) bool_arrays[name][idx] = (value.i != 0);
        else throw std::runtime_error("Undefined array: " + name);
    }

    // Class definition
    else if (auto classdef = dynamic_cast<const ClassDef*>(stmt)) {
        class_defs[classdef->name] = classdef;
        return;
    }

    // Object instantiation
    else if (auto objinst = dynamic_cast<const ObjectInstantiation*>(stmt)) {
        // Create the object instance
        ObjectInstance inst;
        inst.className = objinst->className;
        const ClassDef* classDef = class_defs[objinst->className];
        std::vector<std::pair<std::string, std::string>> allFields;
        collectFields(classDef, allFields);
        for (const auto& field : allFields) {
            if (field.first == "int") inst.fields[field.second] = 0;
            else if (field.first == "float") inst.fields[field.second] = 0.0;
            else if (field.first == "char") inst.fields[field.second] = '\0';
            else if (field.first == "string") inst.fields[field.second] = std::string("");
            else if (field.first == "bool") inst.fields[field.second] = 0;
        }
        objects[objinst->varName] = inst;
        // Call constructor (init method) if arguments are provided
        if (!objinst->arguments.empty()) {
            const FunctionDef* ctor = nullptr;
            std::vector<const FunctionDef*> allMethods;
            collectMethods(classDef, allMethods);
            for (const auto& m : allMethods) {
                if (m->name == "init") { ctor = m; break; }
            }
            if (!ctor) throw std::runtime_error("Constructor 'init' not found in class " + objinst->className);
            if (objinst->arguments.size() != ctor->parameters.size())
                throw std::runtime_error("Constructor argument count mismatch for class " + objinst->className);
            // Push new local scopes
            variables_stack.push_back({});
            float_variables_stack.push_back({});
            char_variables_stack.push_back({});
            string_variables_stack.push_back({});
            // Set up fields as locals
            for (const auto& field : inst.fields) {
                if (std::holds_alternative<int>(field.second)) setIntVar(field.first, std::get<int>(field.second));
                else if (std::holds_alternative<double>(field.second)) setFloatVar(field.first, std::get<double>(field.second));
                else if (std::holds_alternative<char>(field.second)) setCharVar(field.first, std::get<char>(field.second));
                else if (std::holds_alternative<std::string>(field.second)) setStringVar(field.first, std::get<std::string>(field.second));
            }
            // Set up parameters
            for (size_t i = 0; i < objinst->arguments.size(); ++i) {
                Value argVal = evalExpr(objinst->arguments[i].get());
                if (argVal.type == ValueType::FLOAT) setFloatVar(ctor->parameters[i], argVal.f);
                else if (argVal.type == ValueType::CHAR) setCharVar(ctor->parameters[i], argVal.c);
                else if (argVal.type == ValueType::STRING) setStringVar(ctor->parameters[i], argVal.s);
                else setIntVar(ctor->parameters[i], argVal.i);
            }
            // Execute constructor body
            for (const auto& stmt : ctor->body) {
                execute(stmt.get());
            }
            // Update fields from local scope after constructor
            for (auto& field : class_defs[objinst->className]->fields) {
                const std::string& fname = field.second;
                if (string_variables_stack.back().count(fname)) objects[objinst->varName].fields[fname] = string_variables_stack.back()[fname];
                else if (variables_stack.back().count(fname)) objects[objinst->varName].fields[fname] = variables_stack.back()[fname];
                else if (float_variables_stack.back().count(fname)) objects[objinst->varName].fields[fname] = float_variables_stack.back()[fname];
                else if (char_variables_stack.back().count(fname)) objects[objinst->varName].fields[fname] = char_variables_stack.back()[fname];
            }
            // Pop local scopes
            if (variables_stack.size() > 1) variables_stack.pop_back();
            if (float_variables_stack.size() > 1) float_variables_stack.pop_back();
            if (char_variables_stack.size() > 1) char_variables_stack.pop_back();
            if (string_variables_stack.size() > 1) string_variables_stack.pop_back();
        }
        return;
    }

    // Default fallback
    else {
        throw std::runtime_error("Unsupported statement");
    }
}

void run(const std::vector<std::unique_ptr<Statement>>& statements) {
    // First pass: register all class definitions
    for (const auto& stmt : statements) {
        if (dynamic_cast<const ClassDef*>(stmt.get())) {
            execute(stmt.get());
        }
    }

    // Second pass: instantiate all class-based objects (e.g., Person p;)
    for (const auto& stmt : statements) {
        if (auto assign = dynamic_cast<const Assignment*>(stmt.get())) {
            if (class_defs.count(assign->type) && assign->value == nullptr) {
                execute(stmt.get());  // instantiate object
            }
        }
    }


    // Third pass: execute all remaining statements
    for (const auto& stmt : statements) {
        // Skip already handled class definitions and object instantiations
        if (dynamic_cast<const ClassDef*>(stmt.get())) continue;

        if (auto assign = dynamic_cast<const Assignment*>(stmt.get())) {
            if (class_defs.count(assign->type) && assign->value == nullptr) continue;
        }

        execute(stmt.get());
    }
}
