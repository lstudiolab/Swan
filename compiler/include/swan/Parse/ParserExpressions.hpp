#pragma once
#include "swan/Parse/Parser.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace swan {
inline int Parser::precedence(std::string_view op) { return operatorPrecedence(op); }

inline ExprPtr Parser::expression(int minimum) {
    auto left = prefix();
    while (true) {
        const int level = precedence(peek().kind);
        if (level < minimum) break;
        auto& op = advance();
        auto right = expression(op.kind == "=" ? level : level + 1);
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::Binary; node->token = op.location(); node->op = std::move(op.value);
        node->left = std::move(left); node->right = std::move(right); left = std::move(node);
    }
    return left;
}

inline ExprPtr Parser::prefix() {
    if (check("!") || check("-") || check("not")) {
        auto& op = advance();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::Unary; node->token = op.location(); node->op = std::move(op.value);
        node->left = expression(8);
        return node;
    }
    auto& token = advance();
    auto node = std::make_unique<Expr>(); node->token = token.location();
    if (token.kind == "INT" || token.kind == "FLOAT" || token.kind == "STRING" ||
        token.kind == "true" || token.kind == "false" || token.kind == "none") {
        node->kind = Expr::Kind::Literal; node->value = std::move(token.value);
        node->literal_kind = token.kind;
    } else if (token.kind == "IDENT") {
        node->kind = Expr::Kind::Name; node->value = std::move(token.value);
    } else if (token.kind == "(") {
        node = expression(); expect(")","expected ')' after expression");
    } else fail(token,"expected expression");

    while (true) {
        if (match("(")) {
            auto call = std::make_unique<Expr>();
            call->kind = Expr::Kind::Call; call->token = node->token; call->left = std::move(node);
            if (!check(")")) {
                do { call->arguments.push_back(expression()); } while (match(","));
            }
            expect(")","expected ')' after arguments");
            node = std::move(call);
        } else if (match(".")) {
            auto& member = expect("IDENT","expected member name");
            auto access = std::make_unique<Expr>();
            access->kind = Expr::Kind::Member; access->token = member.location();
            access->left = std::move(node); access->value = std::move(member.value); node = std::move(access);
        } else break;
    }
    return node;
}
}
