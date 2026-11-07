#pragma once
#include "swan/Parse/Parser.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
namespace swan {
inline StmtPtr Parser::statement() {
    if (check("if")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::If; node->token = advance();
        node->condition = expression();
        node->body = block();
        if (match("else")) {
            if (check("if")) node->alternate.push_back(statement());
            else node->alternate = block();
        }
        return node;
    }
    if (check("while")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::While; node->token = advance();
        node->condition = expression();
        node->body = block();
        return node;
    }
    if (check("return")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::Return; node->token = advance();
        if (!check("NEWLINE") && !check("DEDENT") && !check(";") && !check("EOF"))
            node->value = expression();
        return node;
    }
    if (check("break") || check("continue") || check("pass") || check("log")) {
        auto node = std::make_unique<Stmt>();
        node->token = advance();
        if (node->token.kind == "break") node->kind = Stmt::Kind::Break;
        else if (node->token.kind == "continue") node->kind = Stmt::Kind::Continue;
        else if (node->token.kind == "log") {
            node->kind = Stmt::Kind::Log; node->value = expression();
        } else node->kind = Stmt::Kind::Pass;
        return node;
    }
    auto node = std::make_unique<Stmt>();
    node->kind = Stmt::Kind::Expression; node->value = expression();
    node->token = node->value->token;
    return node;
}
}
