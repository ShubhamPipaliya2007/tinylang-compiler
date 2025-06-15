#pragma once
#include "lexer.hpp"
#include "ast.hpp"

std::vector<std::unique_ptr<Statement>> parse(const std::vector<Token>& tokens);
