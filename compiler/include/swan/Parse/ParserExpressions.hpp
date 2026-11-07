#pragma once
#include "swan/Parse/Parser.hpp"
#include <memory>
#include <utility>
namespace swan {
inline int Parser::precedence(std::string_view op) { return operatorPrecedence(op); }
inline ExprPtr Parser::expression(int minimum) {
    auto left = prefix();
    while (true) {
        const int level = precedence(peek().kind);
        if (level < minimum) break;
        const auto& op = advance();
        auto right = expression(op.kind == "=" ? level : level + 1);
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::Binary;
        node->token = op;
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}
inline ExprPtr Parser::prefix() {
    if (check("!") || check("-") || check("not")) {
        const auto& op = advance();
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::Unary;
        node->token = op;
        node->left = expression(8);
        return node;
    }
    const auto& token = advance();
    auto node = std::make_unique<Expr>();
    node->token = token;
    if (token.kind == "INT" || token.kind == "FLOAT" || token.kind == "STRING" ||
        token.kind == "true" || token.kind == "false" || token.kind == "none")
        node->kind = Expr::Kind::Literal;
    else if (token.kind == "IDENT")
        node->kind = Expr::Kind::Name;
    else if (token.kind == "(") {
        node = expression();
        expect(")","expected ')' after expression");
    } else fail(token,"expected expression");
    while (true) {
        if (match("(")) {
            auto call = std::make_unique<Expr>();
            call->kind = Expr::Kind::Call;
            call->token = node->token;
            call->left = std::move(node);
            if (!check(")")) {
                do { call->arguments.push_back(expression()); } while (match(","));
            }
            expect(")","expected ')' after arguments");
            node = std::move(call);
        } else if (match("[")) {
            const auto bracket = tokens_[position_ - 1];
            auto index = std::make_unique<Expr>();
            index->kind = Expr::Kind::Index;
            index->token = bracket;
            index->left = std::move(node);
            index->right = expression();
            expect("]","expected ']' after array index");
            node = std::move(index);
        } else if (match(".")) {
            const auto& member = expect("IDENT","expected member name");
            auto access = std::make_unique<Expr>();
            access->kind = Expr::Kind::Member;
            access->token = member;
            access->left = std::move(node);
            node = std::move(access);
        } else break;
    }
    return node;
}
}
