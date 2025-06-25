#include "parser.hpp"
#include <stdexcept>
#include <iostream>
#include <algorithm>

static size_t current = 0;
static std::vector<Token> tokens;
static std::unique_ptr<Statement> parseFunction();
static std::unique_ptr<Expr> parseExpression();
static std::vector<std::string> parseParameterList();
static std::unique_ptr<Statement> parseClass();

// Forward declaration for class name lookup
extern std::unordered_set<std::string> g_class_names;

// Helper to format error messages with line/column
static std::string errorMsg(const std::string& msg, const Token& token) {
    return msg + " at line " + std::to_string(token.line) + ", column " + std::to_string(token.column);
}

static Token peek() {
    return tokens[current];
}

static Token advance() {
    return tokens[current++];
}

bool check(TokenType type) {
    return peek().type == type;
}

static bool match(TokenType type) {
    if (tokens[current].type == type) {
        current++;
        return true;
    }
    return false;
}

static std::unique_ptr<Expr> parseArrayLiteral() {
    std::vector<std::unique_ptr<Expr>> elements;
    if (!match(TokenType::RBRACE)) {
        do {
            elements.push_back(parseExpression());
        } while (match(TokenType::COMMA));
        if (!match(TokenType::RBRACE))
            throw std::runtime_error(errorMsg("Expected '}' after array literal", peek()));
    }
    return std::make_unique<ArrayLiteral>(std::move(elements));
}

static std::unique_ptr<Expr> parsePrimary() {
    Token token = advance();
    // std::cout << "[DEBUG] parsePrimary: current token index=" << current << std::endl;

    switch (token.type) {
        case TokenType::NUMBER:
            return std::make_unique<Number>(std::stoi(token.value));
        case TokenType::FLOAT_LITERAL:
            return std::make_unique<FloatLiteral>(std::stod(token.value));
        case TokenType::CHAR_LITERAL:
            return std::make_unique<CharLiteral>(token.value[0]);
        case TokenType::BOOLEAN_LITERAL:
            return std::make_unique<BoolLiteral>(token.value == "true");
        case TokenType::STRING_LITERAL:
            return std::make_unique<StringLiteral>(token.value);  // ensure lexer stores the string literal correctly
        // case TokenType::NOT:
        //     std::cout << "[DEBUG] Parsing unary NOT" << std::endl;
        //     return std::make_unique<UnaryExpr>(TokenType::NOT, parsePrimary());
        case TokenType::IDENTIFIER: {
            std::string name = token.value;
            std::unique_ptr<Expr> expr = std::make_unique<Variable>(name);
            // Handle array access (possibly chained)
            while (match(TokenType::LBRACKET)) {
                auto index = parseExpression();
                if (!match(TokenType::RBRACKET))
                    throw std::runtime_error(errorMsg("Expected ']' after array index", peek()));
                expr = std::make_unique<ArrayAccess>(name, std::move(index));
                name = ""; // Only use name for first access
            }
            // Handle member access and method calls (chained)
            while (match(TokenType::DOT)) {
                if (peek().type != TokenType::IDENTIFIER)
                    throw std::runtime_error(errorMsg("Expected member name after '.'", peek()));
                std::string member = advance().value;
                // Check for method call
                if (match(TokenType::LPAREN)) {
                    std::vector<std::unique_ptr<Expr>> args;
                    if (!match(TokenType::RPAREN)) {
                        do {
                            args.push_back(parseExpression());
                        } while (match(TokenType::COMMA));
                        if (!match(TokenType::RPAREN)) {
                            throw std::runtime_error(errorMsg("Expected ')' after arguments to method call", peek()));
                        }
                    }
                    expr = std::make_unique<ObjectMethodCall>(std::move(expr), member, std::move(args));
                } else {
                    expr = std::make_unique<ObjectMemberAccess>(std::move(expr), member);
                }
            }
            // Handle function call on base variable (not member)
            if (auto var = dynamic_cast<Variable*>(expr.get())) {
                if (match(TokenType::LPAREN)) {
                    std::vector<std::unique_ptr<Expr>> args;
                    if (!match(TokenType::RPAREN)) {
                        do {
                            args.push_back(parseExpression());
                        } while (match(TokenType::COMMA));
                        if (!match(TokenType::RPAREN)) {
                            throw std::runtime_error(errorMsg("Expected ')' after arguments to function call", peek()));
                        }
                    }
                    expr = std::make_unique<CallExpr>(var->name, std::move(args));
                }
            }
            return expr;
        }
        case TokenType::LBRACE: // Array literal
            return parseArrayLiteral();
        case TokenType::INPUT:{
            if (!match(TokenType::LPAREN) || !match(TokenType::RPAREN))
                throw std::runtime_error(errorMsg("Expected 'input()'", peek()));
            return std::make_unique<InputExpr>();
        }
        case TokenType::READ:{
            if (!match(TokenType::LPAREN)) throw std::runtime_error(errorMsg("Expected '(' after 'read'", peek()));
            if (peek().type != TokenType::STRING_LITERAL)
                throw std::runtime_error(errorMsg("Expected string literal in read()", peek()));
            std::string filename = advance().value;
            if (!match(TokenType::RPAREN)) throw std::runtime_error(errorMsg("Expected ')' after read argument", peek()));
            return std::make_unique<ReadExpr>(filename);
        }
        case TokenType::LPAREN: {
            auto expr = parseExpression();
            if (!match(TokenType::RPAREN))
                throw std::runtime_error(errorMsg("Expected ')' after expression", peek()));
            return expr;
        }

        default:
            throw std::runtime_error(errorMsg("Unexpected token in expression: '" + token.value + "'", token));
    }
}

int getPrecedence(TokenType type) {
    int prec = -1;
    switch (type) {
        case TokenType::MULTIPLICATION:
        case TokenType::DIVISION: prec = 4; break;
        case TokenType::PLUS:
        case TokenType::MINUS: prec = 3; break;
        case TokenType::GREATERTHEN:
        case TokenType::LESSTHEN:
        case TokenType::EQUALTO:
        case TokenType::NOTEQUALTO: prec = 2; break;
        case TokenType::AND: prec = 1; break;
        case TokenType::OR: prec = 0; break;
        case TokenType::NOT: prec = 5; break;
        default: prec = -1; break;
    }
    // std::cout << "[DEBUG] getPrecedence: type=" << (int)type << ", prec=" << prec << std::endl;
    return prec;
}

static std::unique_ptr<Expr> parseUnary() {
    if (match(TokenType::NOT) || match(TokenType::MINUS)) {
        Token op = tokens[current - 1];
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(op.type, std::move(operand));
    }
    return parsePrimary();
}

static std::unique_ptr<Expr> parseBinaryExpr(int minPrec) {
    auto left = parseUnary();
    while (true) {
        TokenType opType = peek().type;
        int prec = getPrecedence(opType);
        if (prec < minPrec) break;
        Token op = advance();
        auto right = parseBinaryExpr(prec + 1);
        left = std::make_unique<BinaryExpr>(std::move(left), op.type, std::move(right));
    }
    return left;
}

static std::unique_ptr<Expr> parseExpression() {
    // std::cout << "[DEBUG] parseExpression: starting" << std::endl;
    return parseBinaryExpr(0);
}

static std::unique_ptr<Expr> parseAssignable() {
    // Parse a variable, array access, or object member access as an assignable target
    if (peek().type == TokenType::IDENTIFIER) {
        std::string name = advance().value;
        std::unique_ptr<Expr> expr = std::make_unique<Variable>(name);
        // Support chained array and member access
        while (true) {
            if (match(TokenType::LBRACKET)) {
                auto index = parseExpression();
                if (!match(TokenType::RBRACKET))
                    throw std::runtime_error(errorMsg("Expected ']' after array index", peek()));
                expr = std::make_unique<ArrayAccess>(name, std::move(index));
                name = ""; // Only use name for first access
            } else if (match(TokenType::DOT)) {
                if (peek().type != TokenType::IDENTIFIER)
                    throw std::runtime_error(errorMsg("Expected member name after '.'", peek()));
                std::string member = advance().value;
                expr = std::make_unique<ObjectMemberAccess>(std::move(expr), member);
            } else {
                break;
            }
        }
        return expr;
    }
    throw std::runtime_error(errorMsg("Invalid assignment target", peek()));
}

static std::unique_ptr<Statement> parseSimpleAssignment() {
    if (match(TokenType::INT) || match(TokenType::FLOAT) || match(TokenType::CHAR)) {
        TokenType varType = tokens[current - 1].type;
        if (peek().type != TokenType::IDENTIFIER)
            throw std::runtime_error(errorMsg("Expected identifier after type", peek()));
        std::string name = advance().value;
        if (!match(TokenType::ASSIGN))
            throw std::runtime_error(errorMsg("Expected '=' after variable name", peek()));
        auto expr = parseExpression();
        return std::make_unique<Assignment>(name, std::move(expr));
    } else if (peek().type == TokenType::IDENTIFIER) {
        // Support assignment to object fields
        auto lhs = parseAssignable();
        if (!match(TokenType::ASSIGN))
            throw std::runtime_error(errorMsg("Expected '=' after assignment target", peek()));
        auto expr = parseExpression();
        // If lhs is Variable, use its name; if ObjectMemberAccess, serialize as 'obj.field'
        if (auto var = dynamic_cast<Variable*>(lhs.get())) {
            return std::make_unique<Assignment>(var->name, std::move(expr));
        } else if (auto objmem = dynamic_cast<ObjectMemberAccess*>(lhs.get())) {
            // Serialize as 'obj.field' for codegen
            std::string target;
            std::vector<std::string> chain;
            auto* cur = objmem;
            while (cur) {
                chain.push_back(cur->member);
                if (auto innerVar = dynamic_cast<Variable*>(cur->object.get())) {
                    chain.push_back(innerVar->name);
                    break;
                } else if (auto innerObj = dynamic_cast<ObjectMemberAccess*>(cur->object.get())) {
                    cur = innerObj;
                } else {
                    throw std::runtime_error("Unsupported assignment target");
                }
            }
            std::reverse(chain.begin(), chain.end());
            for (size_t i = 0; i < chain.size(); ++i) {
                if (i > 0) target += ".";
                target += chain[i];
            }
            return std::make_unique<Assignment>(target, std::move(expr));
        } else {
            throw std::runtime_error("Unsupported assignment target");
        }
    }
    throw std::runtime_error(errorMsg("Invalid assignment or expression", peek()));
}

static std::unique_ptr<Statement> parseStatement() {
    if (match(TokenType::CLASS)) {
        auto classDef = parseClass();
        if (auto cd = dynamic_cast<ClassDef*>(classDef.get())) {
            g_class_names.insert(cd->name);
        }
        return classDef;
    }
    if (match(TokenType::COMEANDDO)) {
        return parseFunction();
    }
    if (match(TokenType::FOR)) {
        if (!match(TokenType::LPAREN)) throw std::runtime_error(errorMsg("Expected '(' after 'for'", peek()));
        std::unique_ptr<Statement> initializer = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            initializer = parseSimpleAssignment();
        }
        if (!match(TokenType::SEMICOLON)) throw std::runtime_error(errorMsg("Expected ';' after initializer", peek()));
        std::unique_ptr<Expr> condition = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            condition = parseExpression();
        }
        if (!match(TokenType::SEMICOLON)) throw std::runtime_error(errorMsg("Expected ';' after condition", peek()));
        std::unique_ptr<Statement> increment = nullptr;
        if (!check(TokenType::RPAREN)) {
            increment = parseSimpleAssignment();
        }
        if (!match(TokenType::RPAREN)) throw std::runtime_error(errorMsg("Expected ')' after increment", peek()));
        if (!match(TokenType::LBRACE)) throw std::runtime_error(errorMsg("Expected '{' after for", peek()));
        std::vector<std::unique_ptr<Statement>> body;
        while (!check(TokenType::RBRACE)) {
            body.push_back(parseStatement());
        }
        match(TokenType::RBRACE);
        return std::make_unique<ForStatement>(
            std::move(initializer),
            std::move(condition),
            std::move(increment),
            std::move(body)
        );
    }
    if (match(TokenType::WHILE)) {
        if (!match(TokenType::LPAREN)) throw std::runtime_error(errorMsg("Expected '(' after while", peek()));
        auto condition = parseExpression();
        if (!match(TokenType::RPAREN)) throw std::runtime_error(errorMsg("Expected ')' after while condition", peek()));
        if (!match(TokenType::LBRACE)) throw std::runtime_error(errorMsg("Expected '{' after while", peek()));
        std::vector<std::unique_ptr<Statement>> body;
        while (!check(TokenType::RBRACE)) {
            body.push_back(parseStatement());
        }
        match(TokenType::RBRACE);
        return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
    }
    if (match(TokenType::IF)) {
        if (!match(TokenType::LPAREN)) throw std::runtime_error(errorMsg("Expected '(' after 'if'", peek()));
        auto condition = parseExpression();
        if (!match(TokenType::RPAREN)) throw std::runtime_error(errorMsg("Expected ')' after condition", peek()));
        if (!match(TokenType::LBRACE)) throw std::runtime_error(errorMsg("Expected '{' after if condition", peek()));
        std::vector<std::unique_ptr<Statement>> thenBlock;
        while (!check(TokenType::RBRACE)) {
            thenBlock.push_back(parseStatement());
        }
        match(TokenType::RBRACE);
        std::vector<std::unique_ptr<Statement>> elseBlock;
        if (match(TokenType::ELSE)) {
            if (!match(TokenType::LBRACE)) throw std::runtime_error(errorMsg("Expected '{' after else", peek()));
            while (!check(TokenType::RBRACE)) {
                elseBlock.push_back(parseStatement());
            }
            match(TokenType::RBRACE);
        }
        return std::make_unique<IfStatement>(
            std::move(condition),
            std::move(thenBlock),
            std::move(elseBlock)
        );
    }
    if (match(TokenType::INT) || match(TokenType::FLOAT) || match(TokenType::CHAR) || match(TokenType::BOOL) || match(TokenType::STRING_TYPE)) {
        TokenType varType = tokens[current - 1].type;
        std::string typeStr;
        switch (varType) {
            case TokenType::INT: typeStr = "int"; break;
            case TokenType::FLOAT: typeStr = "float"; break;
            case TokenType::CHAR: typeStr = "char"; break;
            case TokenType::BOOL: typeStr = "bool"; break;
            case TokenType::STRING_TYPE: typeStr = "string"; break;
            default: typeStr = "int"; break;
        }
        if (peek().type != TokenType::IDENTIFIER)
            throw std::runtime_error(errorMsg("Expected identifier after type", peek()));
        std::string name = advance().value;
        // Array declaration
        if (match(TokenType::LBRACKET)) {
            if (peek().type == TokenType::RBRACKET) { // int arr[]
                advance();
                if (match(TokenType::ASSIGN)) {
                    if (!match(TokenType::LBRACE))
                        throw std::runtime_error(errorMsg("Expected '{' for array initializer", peek()));
                    auto arrLit = parseArrayLiteral();
                    if (!match(TokenType::SEMICOLON))
                        throw std::runtime_error(errorMsg("Expected ';' after array declaration", peek()));
                    // Assignment node for array initialization
                    return std::make_unique<Assignment>(name, std::move(arrLit), typeStr);
                } else {
                    if (!match(TokenType::SEMICOLON))
                        throw std::runtime_error(errorMsg("Expected ';' after array declaration", peek()));
                    // Empty array declaration (size unknown)
                    return std::make_unique<Assignment>(name, nullptr, typeStr);
                }
            } else { // int arr[10]
                auto sizeExpr = parseExpression();
                if (!match(TokenType::RBRACKET))
                    throw std::runtime_error(errorMsg("Expected ']' after array size", peek()));
                if (!match(TokenType::SEMICOLON))
                    throw std::runtime_error(errorMsg("Expected ';' after array declaration", peek()));
                // Assignment node for fixed-size array (sizeExpr as value)
                return std::make_unique<Assignment>(name, std::move(sizeExpr), typeStr);
            }
        }
        // Normal variable assignment
        if (match(TokenType::ASSIGN)) {
            auto expr = parseExpression();
            if (!match(TokenType::SEMICOLON))
                throw std::runtime_error(errorMsg("Expected ';' after assignment", peek()));
            return std::make_unique<Assignment>(name, std::move(expr), typeStr);
        }
        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error(errorMsg("Expected ';' after declaration", peek()));
        return std::make_unique<Assignment>(name, nullptr, typeStr);
    }
    // Print statement
    if (match(TokenType::PRINT)) {
        if (!match(TokenType::LPAREN))
            throw std::runtime_error(errorMsg("Expected '(' after print", peek()));
        auto expr = parseExpression();
        if (!match(TokenType::RPAREN))
            throw std::runtime_error(errorMsg("Expected ')' after print expression", peek()));
        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error(errorMsg("Expected ';' after print", peek()));
        return std::make_unique<Print>(std::move(expr));
    }
    if (match(TokenType::RETURN)) {
        if (peek().type == TokenType::SEMICOLON) {
            match(TokenType::SEMICOLON);
            return std::make_unique<Return>(nullptr);
        }
        auto expr = parseExpression();
        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error(errorMsg("Expected ';' after return", peek()));
        return std::make_unique<Return>(std::move(expr));
    }
    // Assignment to variable, array, or object field: <assignable> = expr;
    if (peek().type == TokenType::IDENTIFIER) {
        size_t save = current;
        auto lhs = parseAssignable();
        if (match(TokenType::ASSIGN)) {
            auto expr = parseExpression();
            if (!match(TokenType::SEMICOLON))
                throw std::runtime_error(errorMsg("Expected ';' after assignment", peek()));
            // If lhs is Variable, use its name; if ObjectMemberAccess or ArrayAccess, serialize as needed
            if (auto var = dynamic_cast<Variable*>(lhs.get())) {
                return std::make_unique<Assignment>(var->name, std::move(expr));
            } else if (auto objmem = dynamic_cast<ObjectMemberAccess*>(lhs.get())) {
                // Serialize as 'obj.field' or 'arr[idx].field'
                std::string target;
                std::vector<std::string> chain;
                auto* cur = objmem;
                while (cur) {
                    chain.push_back(cur->member);
                    if (auto innerVar = dynamic_cast<Variable*>(cur->object.get())) {
                        chain.push_back(innerVar->name);
                        break;
                    } else if (auto innerArr = dynamic_cast<ArrayAccess*>(cur->object.get())) {
                        // Serialize array access as arr[idx]
                        std::string arrTarget = innerArr->arrayName + "[";
                        if (auto num = dynamic_cast<Number*>(innerArr->index.get())) {
                            arrTarget += std::to_string(num->value);
                        } else {
                            // For now, only support constant index
                            throw std::runtime_error("Only constant indices supported in assignment target");
                        }
                        arrTarget += "]";
                        chain.push_back(arrTarget);
                        break;
                    } else if (auto innerObj = dynamic_cast<ObjectMemberAccess*>(cur->object.get())) {
                        cur = innerObj;
                    } else {
                        throw std::runtime_error("Unsupported assignment target");
                    }
                }
                std::reverse(chain.begin(), chain.end());
                for (size_t i = 0; i < chain.size(); ++i) {
                    if (i > 0) target += ".";
                    target += chain[i];
                }
                return std::make_unique<Assignment>(target, std::move(expr));
            } else if (auto arr = dynamic_cast<ArrayAccess*>(lhs.get())) {
                // Serialize as arr[idx]
                std::string target = arr->arrayName + "[";
                if (auto num = dynamic_cast<Number*>(arr->index.get()))
                    target += std::to_string(num->value);
                else
                    target += "?"; // fallback for non-const index
                target += "]";
                return std::make_unique<Assignment>(target, std::move(expr));
            } else {
                throw std::runtime_error("Unsupported assignment target");
            }
        } else {
            current = save; // rewind if not assignment
        }
    }
    // Object array declaration: <ClassName> <var>[<size>];
    if (peek().type == TokenType::IDENTIFIER) {
        std::string typeName = peek().value;
        if (g_class_names.count(typeName)) {
            advance(); // consume type name
            if (peek().type != TokenType::IDENTIFIER)
                throw std::runtime_error(errorMsg("Expected variable name after class type", peek()));
            std::string varName = advance().value;
            if (match(TokenType::LBRACKET)) {
                auto sizeExpr = parseExpression();
                if (!match(TokenType::RBRACKET))
                    throw std::runtime_error(errorMsg("Expected ']' after array size", peek()));
                if (match(TokenType::SEMICOLON)) {
                    // Object array declaration
                    return std::make_unique<Assignment>(varName, std::move(sizeExpr), typeName + "[]");
                } else {
                    throw std::runtime_error(errorMsg("Expected ';' after object array declaration", peek()));
                }
            }
            // ... existing constructor and default instantiation logic ...
            std::vector<std::unique_ptr<Expr>> args;
            if (match(TokenType::LPAREN)) {
                if (!match(TokenType::RPAREN)) {
                    do {
                        args.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                    if (!match(TokenType::RPAREN))
                        throw std::runtime_error(errorMsg("Expected ')' after constructor arguments", peek()));
                }
            }
            if (!match(TokenType::SEMICOLON))
                throw std::runtime_error(errorMsg("Expected ';' after object declaration", peek()));
            if (!args.empty()) {
                return std::make_unique<ObjectInstantiation>(typeName, varName, std::move(args));
            } else {
                return std::make_unique<Assignment>(varName, nullptr, typeName);
            }
        }
    }
    // Fallback: expression statement
    auto expr = parseExpression();
    if (!match(TokenType::SEMICOLON))
        throw std::runtime_error(errorMsg("Expected ';' after expression", peek()));
    return std::make_unique<ExprStatement>(std::move(expr));
}

std::vector<std::unique_ptr<Statement>> parse(const std::vector<Token>& inputTokens) {
    tokens = inputTokens;
    current = 0;
    
    // Debug: Print all tokens
    // std::cout << "[DEBUG] All tokens:" << std::endl;
    // for (size_t i = 0; i < tokens.size(); i++) {
    //     std::cout << "[DEBUG] Token " << i << ": type=" << (int)tokens[i].type << ", value='" << tokens[i].value << "'" << std::endl;
    // }
    
    std::vector<std::unique_ptr<Statement>> statements;
    while (peek().type != TokenType::END) {
        statements.push_back(parseStatement());
    }
    return statements;
}

static std::vector<std::string> parseParameterList() {
    std::vector<std::string> params;
    if (!match(TokenType::RPAREN)) {
        do {
            // Support typed parameters: <type> <name>
            if (peek().type == TokenType::INT || peek().type == TokenType::FLOAT || peek().type == TokenType::CHAR || peek().type == TokenType::BOOL || peek().type == TokenType::STRING_TYPE) {
                advance(); // skip type
            }
            if (peek().type != TokenType::IDENTIFIER)
                throw std::runtime_error(errorMsg("Expected parameter name", peek()));
            params.push_back(advance().value);
        } while (match(TokenType::COMMA));
        if (!match(TokenType::RPAREN))
            throw std::runtime_error(errorMsg("Expected ')'", peek()));
    }
    return params;
}

static std::unique_ptr<Statement> parseFunction() {
    if (peek().type != TokenType::IDENTIFIER)
        throw std::runtime_error(errorMsg("Expected function name after 'ComeAndDo'", peek()));
    std::string name = advance().value;
    if (!match(TokenType::LPAREN))
        throw std::runtime_error(errorMsg("Expected '(' after function name", peek()));
    auto parameters = parseParameterList();
    if (!match(TokenType::LBRACE))
        throw std::runtime_error(errorMsg("Expected '{' to begin function body", peek()));
    std::vector<std::unique_ptr<Statement>> bodyStmts;
    while (!check(TokenType::RBRACE)) {
        bodyStmts.push_back(parseStatement());
    }
    if (!match(TokenType::RBRACE))
        throw std::runtime_error(errorMsg("Expected '}' after function body", peek()));
    return std::make_unique<FunctionDef>(
        name,
        std::move(parameters),
        std::move(bodyStmts)
    );
}

static std::unique_ptr<Statement> parseClass() {
    if (peek().type != TokenType::IDENTIFIER)
        throw std::runtime_error(errorMsg("Expected class name after 'class'", peek()));
    std::string className = advance().value;
    std::string baseClass;
    if (match(TokenType::COLON)) {
        if (peek().type != TokenType::IDENTIFIER)
            throw std::runtime_error(errorMsg("Expected base class name after ':'", peek()));
        baseClass = advance().value;
    }
    if (!match(TokenType::LBRACE))
        throw std::runtime_error(errorMsg("Expected '{' after class name", peek()));
    std::vector<std::pair<std::string, std::string>> fields;
    std::vector<std::unique_ptr<FunctionDef>> methods;
    while (!check(TokenType::RBRACE)) {
        // Parse field: <type> <name>;
        if (match(TokenType::INT) || match(TokenType::FLOAT) || match(TokenType::CHAR) || match(TokenType::BOOL) || match(TokenType::STRING_TYPE)) {
            TokenType typeTok = tokens[current - 1].type;
            std::string typeStr;
            switch (typeTok) {
                case TokenType::INT: typeStr = "int"; break;
                case TokenType::FLOAT: typeStr = "float"; break;
                case TokenType::CHAR: typeStr = "char"; break;
                case TokenType::BOOL: typeStr = "bool"; break;
                case TokenType::STRING_TYPE: typeStr = "string"; break;
                default: typeStr = "int"; break;
            }
            if (peek().type != TokenType::IDENTIFIER)
                throw std::runtime_error(errorMsg("Expected field name after type in class", peek()));
            std::string fieldName = advance().value;
            if (!match(TokenType::SEMICOLON))
                throw std::runtime_error(errorMsg("Expected ';' after field declaration in class", peek()));
            fields.emplace_back(typeStr, fieldName);
        } else if (match(TokenType::COMEANDDO)) {
            // Parse method (reuse function parser)
            if (peek().type != TokenType::IDENTIFIER)
                throw std::runtime_error(errorMsg("Expected method name after 'ComeAndDo' in class", peek()));
            std::string methodName = advance().value;
            if (!match(TokenType::LPAREN))
                throw std::runtime_error(errorMsg("Expected '(' after method name", peek()));
            auto parameters = parseParameterList();
            if (!match(TokenType::LBRACE))
                throw std::runtime_error(errorMsg("Expected '{' to begin method body", peek()));
            std::vector<std::unique_ptr<Statement>> bodyStmts;
            while (!check(TokenType::RBRACE)) {
                bodyStmts.push_back(parseStatement());
            }
            if (!match(TokenType::RBRACE))
                throw std::runtime_error(errorMsg("Expected '}' after method body", peek()));
            methods.push_back(std::make_unique<FunctionDef>(methodName, std::move(parameters), std::move(bodyStmts)));
        } else {
            throw std::runtime_error(errorMsg("Unexpected token in class body", peek()));
        }
    }
    if (!match(TokenType::RBRACE))
        throw std::runtime_error(errorMsg("Expected '}' after class body", peek()));
    return std::make_unique<ClassDef>(className, baseClass, std::move(fields), std::move(methods));
}
