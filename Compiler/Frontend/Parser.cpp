#include "Swan/Frontend/Parser.hpp"

#include <utility>

namespace swan {

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

const std::vector<ParserError>& Parser::errors() const noexcept { return errors_; }

const Token& Parser::peek(std::size_t lookahead) const noexcept {
    const auto index = current_ + lookahead;
    return index < tokens_.size() ? tokens_[index] : tokens_.back();
}
const Token& Parser::previous() const noexcept {
    return tokens_[current_ == 0 ? 0 : current_ - 1];
}
bool Parser::atEnd() const noexcept { return check(TokenKind::EndOfFile); }
bool Parser::check(TokenKind kind) const noexcept { return peek().kind == kind; }

bool Parser::match(TokenKind kind) noexcept {
    if (!check(kind)) return false;
    advance();
    return true;
}
const Token& Parser::advance() noexcept {
    if (!atEnd()) ++current_;
    return previous();
}
bool Parser::consume(TokenKind kind, const char* message) {
    if (check(kind)) { advance(); return true; }
    report(peek(), message);
    return false;
}
void Parser::report(const Token& token, std::string message) {
    errors_.push_back({token.location, std::move(message)});
}
void Parser::skipNewlines() noexcept {
    while (match(TokenKind::Newline)) {}
}
void Parser::finishStatement() {
    if (match(TokenKind::Semicolon)) {
        while (match(TokenKind::Semicolon)) {}
        if (!check(TokenKind::Newline) && !atEnd() && !check(TokenKind::Dedent))
            report(peek(), "Expected a newline after ';'.");
    }
    if (check(TokenKind::Newline)) skipNewlines();
    else if (!atEnd() && !check(TokenKind::Dedent) && !check(TokenKind::Else))
        report(peek(), "Expected the end of the statement.");
}
void Parser::synchronize() {
    while (!atEnd() && !check(TokenKind::Newline) && !check(TokenKind::Dedent)) advance();
    if (check(TokenKind::Newline)) skipNewlines();
}

Module Parser::parse() {
    Module module;
    skipNewlines();
    while (!atEnd()) {
        const auto before = current_;
        if (auto statement = declaration()) module.statements.push_back(std::move(statement));
        if (current_ == before) advance();
        skipNewlines();
    }
    return module;
}

StmtPtr Parser::declaration() {
    if (match(TokenKind::Async)) {
        const auto start = previous().location;
        if (match(TokenKind::Fn)) return functionDeclaration(true, start);
        report(previous(), "'async' must be followed by 'fn'.");
        synchronize();
        return nullptr;
    }
    if (match(TokenKind::Fn)) return functionDeclaration(false, previous().location);
    if (match(TokenKind::Struct)) return structDeclaration();
    if (check(TokenKind::Let) || check(TokenKind::Var) || check(TokenKind::Const))
        return variableDeclaration();
    if (match(TokenKind::If)) return ifStatement();
    if (match(TokenKind::While)) return whileStatement();
    if (match(TokenKind::For)) return forStatement();
    if (match(TokenKind::Return)) return returnStatement();
    if (match(TokenKind::Import)) return importStatement();
    if (match(TokenKind::Break) || match(TokenKind::Continue) || match(TokenKind::Pass)) {
        const auto token = previous();
        finishStatement();
        return std::make_unique<ControlStmt>(token.kind, token.location);
    }
    return expressionStatement();
}

StmtPtr Parser::functionDeclaration(bool isAsync, SourceLocation start) {
    if (!consume(TokenKind::Identifier, "Expected a function name.")) {
        synchronize();
        return nullptr;
    }
    const std::string name = previous().lexeme;
    if (!consume(TokenKind::LeftParen, "Expected '(' after function name.")) {
        synchronize();
        return nullptr;
    }

    std::vector<std::string> parameters;
    if (!check(TokenKind::RightParen)) {
        do {
            if (!consume(TokenKind::Identifier, "Expected a parameter name.")) break;
            parameters.push_back(previous().lexeme);
            if (match(TokenKind::Colon)) {
                if (check(TokenKind::Identifier) || check(TokenKind::Int)) advance();
                else report(peek(), "Expected a parameter type after ':'.");
            }
        } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RightParen, "Expected ')' after parameters.");

    std::optional<std::string> returnType;
    if (match(TokenKind::Arrow)) {
        if (check(TokenKind::Identifier) || check(TokenKind::Int)) {
            returnType = advance().lexeme;
        } else report(peek(), "Expected a return type after '->'.");
    }
    auto body = blockAfterColon();
    return std::make_unique<FunctionDecl>(name, std::move(parameters),
        std::move(returnType), std::move(body), isAsync, start);
}

StmtPtr Parser::structDeclaration() {
    const auto start = previous().location;
    if (!consume(TokenKind::Identifier, "Expected a struct name.")) {
        synchronize();
        return nullptr;
    }
    const auto name = previous().lexeme;
    auto members = blockAfterColon();
    return std::make_unique<StructDecl>(name, std::move(members), start);
}

StmtPtr Parser::variableDeclaration() {
    const auto declaration = advance();
    const auto start = declaration.location;
    if (!consume(TokenKind::Identifier, "Expected a variable name.")) {
        synchronize();
        return nullptr;
    }
    const auto name = previous().lexeme;
    std::optional<std::string> typeName;
    if (match(TokenKind::Colon)) {
        if (check(TokenKind::Identifier) || check(TokenKind::Int))
            typeName = advance().lexeme;
        else report(peek(), "Expected a type name after ':'.");
    }
    ExprPtr initializer;
    if (match(TokenKind::Assign)) initializer = expression();
    else if (declaration.kind == TokenKind::Let || declaration.kind == TokenKind::Const)
        report(peek(), "Immutable declarations require an initializer.");
    finishStatement();
    return std::make_unique<VarDecl>(declaration.kind, name, std::move(typeName),
                                     std::move(initializer), start);
}

std::vector<StmtPtr> Parser::blockAfterColon() {
    std::vector<StmtPtr> body;
    consume(TokenKind::Colon, "Expected ':' before the indented block.");
    if (!match(TokenKind::Newline)) {
        report(peek(), "Expected a newline after ':'.");
        return body;
    }
    skipNewlines();
    if (!match(TokenKind::Indent)) {
        report(peek(), "Expected an indented block.");
        return body;
    }

    while (!atEnd() && !check(TokenKind::Dedent)) {
        const auto before = current_;
        if (auto statement = declaration()) body.push_back(std::move(statement));
        if (current_ == before) advance();
        skipNewlines();
    }
    consume(TokenKind::Dedent, "Expected the end of the indented block.");
    return body;
}

StmtPtr Parser::ifStatement() {
    const auto start = previous().location;
    auto condition = expression();
    auto body = blockAfterColon();
    std::vector<StmtPtr> elseBody;
    if (match(TokenKind::Else)) elseBody = blockAfterColon();
    return std::make_unique<IfStmt>(std::move(condition), std::move(body),
                                    std::move(elseBody), start);
}

StmtPtr Parser::whileStatement() {
    const auto start = previous().location;
    auto condition = expression();
    auto body = blockAfterColon();
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body), start);
}

StmtPtr Parser::forStatement() {
    const auto start = previous().location;
    if (!consume(TokenKind::Identifier, "Expected an iteration variable after 'for'.")) {
        synchronize();
        return nullptr;
    }
    const auto variable = previous().lexeme;
    if (!(check(TokenKind::Identifier) && peek().lexeme == "in")) {
        report(peek(), "Expected 'in' after the iteration variable.");
    } else advance();
    auto sequence = expression();
    auto body = blockAfterColon();
    return std::make_unique<ForStmt>(variable, std::move(sequence), std::move(body), start);
}

StmtPtr Parser::returnStatement() {
    const auto start = previous().location;
    ExprPtr value;
    if (!check(TokenKind::Newline) && !check(TokenKind::Semicolon) &&
        !check(TokenKind::Dedent) && !atEnd()) value = expression();
    finishStatement();
    return std::make_unique<ReturnStmt>(std::move(value), start);
}

StmtPtr Parser::importStatement() {
    const auto start = previous().location;
    if (!check(TokenKind::Identifier) && !check(TokenKind::String)) {
        report(peek(), "Expected a module name after 'import'.");
        synchronize();
        return nullptr;
    }
    auto module = advance().lexeme;
    std::optional<std::string> alias;
    if (match(TokenKind::As)) {
        if (consume(TokenKind::Identifier, "Expected an alias after 'as'."))
            alias = previous().lexeme;
    }
    finishStatement();
    return std::make_unique<ImportStmt>(std::move(module), std::move(alias), start);
}

StmtPtr Parser::expressionStatement() {
    const auto start = peek().location;
    auto value = expression();
    if (!value) { synchronize(); return nullptr; }
    finishStatement();
    return std::make_unique<ExprStmt>(std::move(value), start);
}

ExprPtr Parser::expression() { return parsePrecedence(1); }

bool Parser::isAssignment(TokenKind kind) const noexcept {
    return kind == TokenKind::Assign;
}

int Parser::precedence(TokenKind kind) const noexcept {
    switch (kind) {
        case TokenKind::Assign: return 1;
        case TokenKind::Or: return 2;
        case TokenKind::And: return 3;
        case TokenKind::Equal: case TokenKind::NotEqual: return 4;
        case TokenKind::Less: case TokenKind::LessEqual:
        case TokenKind::Greater: case TokenKind::GreaterEqual: return 5;
        case TokenKind::Plus: case TokenKind::Minus: return 6;
        case TokenKind::Star: case TokenKind::Slash: case TokenKind::Percent: return 7;
        default: return 0;
    }
}

ExprPtr Parser::parsePrecedence(int minimumPrecedence) {
    auto left = prefixExpression();
    if (!left) return nullptr;
    left = postfix(std::move(left));

    while (true) {
        const auto opKind = peek().kind;
        const int level = precedence(opKind);
        if (level < minimumPrecedence || level == 0) break;
        const auto op = advance();
        const int nextMinimum = isAssignment(opKind) ? level : level + 1;
        auto right = parsePrecedence(nextMinimum);
        if (!right) {
            report(peek(), "Expected an expression after the operator.");
            break;
        }
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
    }
    return left;
}

ExprPtr Parser::prefixExpression() {
    if (check(TokenKind::Minus) || check(TokenKind::Not) ||
        check(TokenKind::Await) || check(TokenKind::Get) ||
        check(TokenKind::Log) || check(TokenKind::Hold) ||
        check(TokenKind::Store) || check(TokenKind::Clone) ||
        check(TokenKind::Release) || check(TokenKind::Operate) ||
        check(TokenKind::Sync)) {
        const auto op = advance();
        auto operand = parsePrecedence(8);
        if (!operand) {
            report(peek(), "Expected an operand after the prefix operator.");
            return nullptr;
        }
        return std::make_unique<UnaryExpr>(op, std::move(operand));
    }
    return primary();
}

ExprPtr Parser::primary() {
    if (check(TokenKind::Identifier)) {
        const auto token = advance();
        return std::make_unique<NameExpr>(token.lexeme, token.location);
    }
    if (check(TokenKind::Integer) || check(TokenKind::Float) ||
        check(TokenKind::String) || check(TokenKind::True) ||
        check(TokenKind::False) || check(TokenKind::None)) {
        return std::make_unique<LiteralExpr>(advance());
    }
    if (match(TokenKind::LeftParen)) {
        auto value = expression();
        consume(TokenKind::RightParen, "Expected ')' after expression.");
        return value;
    }
    if (match(TokenKind::LeftBracket)) {
        const auto start = previous().location;
        std::vector<ExprPtr> elements;
        if (!check(TokenKind::RightBracket)) {
            do {
                auto element = expression();
                if (element) elements.push_back(std::move(element));
            } while (match(TokenKind::Comma));
        }
        consume(TokenKind::RightBracket, "Expected ']' after array elements.");
        return std::make_unique<ArrayExpr>(std::move(elements), start);
    }
    report(peek(), "Expected an expression.");
    return nullptr;
}

ExprPtr Parser::postfix(ExprPtr value) {
    while (value) {
        if (match(TokenKind::Dot)) {
            if (!consume(TokenKind::Identifier, "Expected a member name after '.'."))
                return value;
            value = std::make_unique<MemberExpr>(std::move(value), previous());
            continue;
        }
        if (match(TokenKind::LeftParen)) {
            const auto start = previous().location;
            std::vector<ExprPtr> arguments;
            if (!check(TokenKind::RightParen)) {
                do {
                    auto argument = expression();
                    if (argument) arguments.push_back(std::move(argument));
                } while (match(TokenKind::Comma));
            }
            consume(TokenKind::RightParen, "Expected ')' after call arguments.");
            value = std::make_unique<CallExpr>(std::move(value), std::move(arguments), start);
            continue;
        }
        break;
    }
    return value;
}

} // namespace swan
