#pragma once
#include "lexer.hpp"
#include "ast.hpp"
#include <unordered_set>

extern std::unordered_set<std::string> g_class_names;

std::vector<std::unique_ptr<Statement>> parse(const std::vector<Token>& tokens);
