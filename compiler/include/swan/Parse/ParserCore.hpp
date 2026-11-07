#pragma once
#include "swan/Parse/Parser.hpp"
#include <string>
#include <utility>

namespace swan {
inline Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {
    if (tokens_.empty() || tokens_.back().kind != "EOF")
        tokens_.push_back(Token{"EOF","",1,1});
}
inline const Token& Parser::peek(std::size_t offset) const {
    const auto position = position_ + offset;
    return position < tokens_.size() ? tokens_[position] : tokens_.back();
}
inline bool Parser::check(std::string_view kind) const { return peek().kind == kind; }
inline bool Parser::match(std::string_view kind) {
    if (!check(kind)) return false;
    advance(); return true;
}
inline const Token& Parser::advance() {
    const Token& token = peek();
    if (token.kind != "EOF") ++position_;
    return token;
}
inline const Token& Parser::expect(std::string_view kind, std::string_view message) {
    if (check(kind)) return advance();
    if (message.empty()) fail(peek(),std::string("expected ") + std::string(kind));
    fail(peek(),message);
}
inline void Parser::separators() { while (match("NEWLINE") || match(";")) {} }
inline std::string Parser::typeName() {
    const auto& token = advance();
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
    expect("NEWLINE","expected a new indented block");
    expect("INDENT","expected indentation to begin block");
    std::vector<StmtPtr> body;
    separators();
    while (!check("DEDENT") && !check("EOF")) {
        const auto before = position_;
        body.push_back(declaration());
        separators();
        if (before == position_) fail(peek(),"parser made no progress");
    }
    expect("DEDENT","expected indentation to close block");
    return body;
}
}
