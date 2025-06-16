#include "parser.hpp"
#include <stdexcept>
#include <iostream>

static size_t current = 0;
static std::vector<Token> tokens;
static std::unique_ptr<Statement> parseFunction();
static std::unique_ptr<Expr> parseExpression();
static std::vector<std::string> parseParameterList();

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

static std::unique_ptr<Expr> parsePrimary() {
    Token token = advance();

    switch (token.type) {
        case TokenType::NUMBER:
            return std::make_unique<Number>(std::stoi(token.value));

        case TokenType::BOOLEAN_LITERAL:
            return std::make_unique<BoolLiteral>(token.value == "true");

        case TokenType::STRING_LITERAL:
            return std::make_unique<StringLiteral>(token.value);  // ensure lexer stores the string literal correctly

        case TokenType::IDENTIFIER: {
            std::string name = token.value;
            if (match(TokenType::LPAREN)) {
                std::vector<std::unique_ptr<Expr>> args;
                if (!match(TokenType::RPAREN)) {
                    do {
                        args.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                    if (!match(TokenType::RPAREN)) {
                        throw std::runtime_error("Expected ')' after arguments to function call");
                    }
                }
                return std::make_unique<CallExpr>(name, std::move(args));
            }
            return std::make_unique<Variable>(name);
        }
        case TokenType::INPUT:{
            if (!match(TokenType::LPAREN) || !match(TokenType::RPAREN))
                throw std::runtime_error("Expected 'input()'");
            return std::make_unique<InputExpr>();
        }
        case TokenType::READ:{
            if (!match(TokenType::LPAREN)) throw std::runtime_error("Expected '(' after 'read'");
            if (peek().type != TokenType::STRING_LITERAL)
                throw std::runtime_error("Expected string literal in read()");
            std::string filename = advance().value;
            if (!match(TokenType::RPAREN)) throw std::runtime_error("Expected ')' after read argument");
            return std::make_unique<ReadExpr>(filename);
        }
        case TokenType::LPAREN: {
            auto expr = parseExpression();
            if (!match(TokenType::RPAREN))
                throw std::runtime_error("Expected ')' after expression");
            return expr;
        }

        default:
            throw std::runtime_error("Unexpected token in expression: '" + token.value + "'");
    }
}

int getPrecedence(TokenType type) {
    switch (type) {
        case TokenType::MULTIPLICATION:
        case TokenType::DIVISION: return 2;
        case TokenType::PLUS:
        case TokenType::MINUS: return 1;
        case TokenType::GREATERTHEN:
        case TokenType::LESSTHEN:
        case TokenType::EQUALTO:
        case TokenType::NOTEQUALTO: return 1;
        default: return 0;
    }
}

static std::unique_ptr<Expr> parseBinaryExpr(int minPrec) {
    auto left = parsePrimary();
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
    return parseBinaryExpr(1);
}

static std::unique_ptr<Statement> parseSimpleAssignment() {
    if (match(TokenType::INT)) {
        if (peek().type != TokenType::IDENTIFIER)
            throw std::runtime_error("Expected identifier after 'int'");
        std::string name = advance().value;
        if (!match(TokenType::ASSIGN))
            throw std::runtime_error("Expected '=' after variable name");
        auto expr = parseExpression();
        return std::make_unique<Assignment>(name, std::move(expr));
    } else if (peek().type == TokenType::IDENTIFIER) {
        std::string name = advance().value;
        if (!match(TokenType::ASSIGN))
            throw std::runtime_error("Expected '=' after variable name");
        auto expr = parseExpression();
        return std::make_unique<Assignment>(name, std::move(expr));
    }
    throw std::runtime_error("Invalid assignment or expression");
}

static std::unique_ptr<Statement> parseStatement() {
    if (match(TokenType::COMEANDDO)) {
        return parseFunction();
    }
    if (match(TokenType::FOR)) {
        if (!match(TokenType::LPAREN)) throw std::runtime_error("Expected '(' after 'for'");
        std::unique_ptr<Statement> initializer = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            initializer = parseSimpleAssignment();
        }
        if (!match(TokenType::SEMICOLON)) throw std::runtime_error("Expected ';' after initializer");
        std::unique_ptr<Expr> condition = nullptr;
        if (!check(TokenType::SEMICOLON)) {
            condition = parseExpression();
        }
        if (!match(TokenType::SEMICOLON)) throw std::runtime_error("Expected ';' after condition");
        std::unique_ptr<Statement> increment = nullptr;
        if (!check(TokenType::RPAREN)) {
            increment = parseSimpleAssignment();
        }
        if (!match(TokenType::RPAREN)) throw std::runtime_error("Expected ')' after increment");
        if (!match(TokenType::LBRACE)) throw std::runtime_error("Expected '{' after for");
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
        if (!match(TokenType::LPAREN)) throw std::runtime_error("Expected '(' after while");
        auto condition = parseExpression();
        if (!match(TokenType::RPAREN)) throw std::runtime_error("Expected ')' after while condition");
        if (!match(TokenType::LBRACE)) throw std::runtime_error("Expected '{' after while");
        std::vector<std::unique_ptr<Statement>> body;
        while (!check(TokenType::RBRACE)) {
            body.push_back(parseStatement());
        }
        match(TokenType::RBRACE);
        return std::make_unique<WhileStatement>(std::move(condition), std::move(body));
    }
    if (match(TokenType::IF)) {
        if (!match(TokenType::LPAREN)) throw std::runtime_error("Expected '(' after 'if'");
        auto condition = parseExpression();
        if (!match(TokenType::RPAREN)) throw std::runtime_error("Expected ')' after condition");
        if (!match(TokenType::LBRACE)) throw std::runtime_error("Expected '{' after if condition");
        std::vector<std::unique_ptr<Statement>> thenBlock;
        while (!check(TokenType::RBRACE)) {
            thenBlock.push_back(parseStatement());
        }
        match(TokenType::RBRACE);
        std::vector<std::unique_ptr<Statement>> elseBlock;
        if (match(TokenType::ELSE)) {
            if (!match(TokenType::LBRACE)) throw std::runtime_error("Expected '{' after else");
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
    // Bool declaration
    if (match(TokenType::BOOL)) {
        if (peek().type != TokenType::IDENTIFIER)
            throw std::runtime_error("Expected identifier after 'bool'");
        std::string name = advance().value;

        if (!match(TokenType::ASSIGN))
            throw std::runtime_error("Expected '=' after variable name");

        auto expr = parseExpression();

        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error("Expected ';' after expression");

        return std::make_unique<Assignment>(name, std::move(expr));
    }

    // String declaration
    if (match(TokenType::STRING_TYPE)) {
        if (peek().type != TokenType::IDENTIFIER)
            throw std::runtime_error("Expected identifier after 'string'");
        std::string name = advance().value;

        if (!match(TokenType::ASSIGN))
            throw std::runtime_error("Expected '=' after variable name");

        auto expr = parseExpression();

        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error("Expected ';' after expression");

        return std::make_unique<Assignment>(name, std::move(expr));
    }

    if (match(TokenType::INT) || match(TokenType::BOOL) || match(TokenType::STRING_TYPE)) {
        TokenType varType = tokens[current - 1].type;
    
        if (peek().type != TokenType::IDENTIFIER)
            throw std::runtime_error("Expected identifier after type");
    
        std::string name = advance().value;
    
        if (!match(TokenType::ASSIGN))
            throw std::runtime_error("Expected '=' after variable name");
    
        auto expr = parseExpression();
    
        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error("Expected ';' after assignment");
    
        return std::make_unique<Assignment>(name, std::move(expr));  // Optionally store varType too
    }    
    if (match(TokenType::PRINT)) {
        if (!match(TokenType::LPAREN))
            throw std::runtime_error("Expected '(' after print");
        auto expr = parseExpression();
        if (!match(TokenType::RPAREN))
            throw std::runtime_error("Expected ')' after print expression");
        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error("Expected ';' after print");
        return std::make_unique<Print>(std::move(expr));
    }
    if (match(TokenType::RETURN)) {
        auto expr = parseExpression();
        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error("Expected ';' after return");
        return std::make_unique<Return>(std::move(expr));
    }
    if (peek().type == TokenType::IDENTIFIER && tokens[current + 1].type == TokenType::ASSIGN) {
        std::string name = advance().value;
        match(TokenType::ASSIGN);
        auto expr = parseExpression();
        if (!match(TokenType::SEMICOLON))
            throw std::runtime_error("Expected ';' after assignment");
        return std::make_unique<Assignment>(name, std::move(expr));
    }
    auto expr = parseExpression();
    if (!match(TokenType::SEMICOLON))
        throw std::runtime_error("Expected ';' after expression");
    return std::make_unique<ExprStatement>(std::move(expr));
}

std::vector<std::unique_ptr<Statement>> parse(const std::vector<Token>& inputTokens) {
    tokens = inputTokens;
    current = 0;
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
            if (peek().type != TokenType::IDENTIFIER)
                throw std::runtime_error("Expected parameter name");
            params.push_back(advance().value);
        } while (match(TokenType::COMMA));
        if (!match(TokenType::RPAREN))
            throw std::runtime_error("Expected ')'");
    }
    return params;
}

static std::unique_ptr<Statement> parseFunction() {
    if (peek().type != TokenType::IDENTIFIER)
        throw std::runtime_error("Expected function name after 'ComeAndDo'");
    std::string name = advance().value;
    if (!match(TokenType::LPAREN))
        throw std::runtime_error("Expected '(' after function name");
    auto parameters = parseParameterList();
    if (!match(TokenType::LBRACE))
        throw std::runtime_error("Expected '{' to begin function body");
    std::vector<std::unique_ptr<Statement>> bodyStmts;
    while (!check(TokenType::RBRACE)) {
        bodyStmts.push_back(parseStatement());
    }
    if (!match(TokenType::RBRACE))
        throw std::runtime_error("Expected '}' after function body");
    return std::make_unique<FunctionDef>(
        name,
        std::move(parameters),
        std::move(bodyStmts)
    );
}
