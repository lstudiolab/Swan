#pragma once
#include "swan/Parse/Parser.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace swan {
inline Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {
    if (tokens_.empty() || tokens_.back().kind != "EOF")
        tokens_.push_back(Token{"EOF","",1,1});
}
inline const Token& Parser::peek(std::size_t offset) const {
    return tokens_[std::min(position_ + offset,tokens_.size() - 1)];
}
inline bool Parser::check(std::string_view kind) const { return peek().kind == kind; }
inline bool Parser::match(std::string_view kind) {
    if (!check(kind)) return false;
    advance(); return true;
}
inline Token Parser::advance() {
    Token token = peek();
    if (!check("EOF")) ++position_;
    return token;
}
inline Token Parser::expect(std::string_view kind, std::string_view message) {
    if (check(kind)) return advance();
    if (message.empty()) fail(peek(),std::string("expected ") + std::string(kind));
    fail(peek(),message);
}
inline void Parser::separators() { while (match("NEWLINE") || match(";")) {} }
inline std::string Parser::typeName() {
    auto token = advance();
    if (token.kind != "IDENT" && token.kind != "int" && token.kind != "float" &&
        token.kind != "bool" && token.kind != "string" && token.kind != "void")
        fail(token,"expected type name");
    return token.value;
}
inline Program Parser::parse() {
    Program program;
    separators();
    while (!check("EOF")) {
        const auto before = position_;
        program.push_back(declaration());
        separators();
        if (before == position_) fail(peek(),"parser made no progress");
    }
    return program;
}
inline std::vector<StmtPtr> Parser::block() {
    expect("{","expected '{' to begin block");
    std::vector<StmtPtr> body;
    separators();
    while (!check("}") && !check("EOF")) {
        const auto before = position_;
        body.push_back(declaration());
        separators();
        if (before == position_) fail(peek(),"parser made no progress");
    }
    expect("}","expected '}' to close block");
    return body;
}
}
