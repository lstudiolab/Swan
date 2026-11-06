#include "swan/Parser.hpp"
#include <algorithm>
#include <utility>

namespace swan {
Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}
const Token& Parser::peek(std::size_t offset) const {
    return tokens_[std::min(position_ + offset,tokens_.size() - 1)];
}
bool Parser::check(std::string_view kind) const { return peek().kind == kind; }
bool Parser::match(std::string_view kind) {
    if (!check(kind)) return false;
    advance(); return true;
}
Token Parser::advance() {
    Token token = peek();
    if (!check("EOF")) ++position_;
    return token;
}
Token Parser::expect(std::string_view kind, std::string_view message) {
    if (check(kind)) return advance();
    fail(peek(),message.empty() ? "expected " + std::string(kind) : message);
}
void Parser::separators() { while (match("NEWLINE") || match(";")) {} }
std::string Parser::typeName() {
    auto token = advance();
    if (token.kind != "IDENT" && token.kind != "int" && token.kind != "float" &&
        token.kind != "bool" && token.kind != "string" && token.kind != "void")
        fail(token,"expected type name");
    return token.value;
}
Program Parser::parse() {
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
std::vector<StmtPtr> Parser::block() {
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
StmtPtr Parser::declaration() {
    if (check("fn")) {
        auto node = std::make_shared<Stmt>();
        node->kind = Stmt::Kind::Function; node->token = advance();
        node->name = expect("IDENT","expected function name").value;
        expect("(");
        if (!check(")")) {
            do {
                auto parameter = expect("IDENT","expected parameter name");
                std::string type;
                if (match(":")) type = typeName();
                node->parameters.push_back({parameter.value,type,parameter});
            } while (match(","));
        }
        expect(")");
        if (match("->")) node->return_type = typeName();
        node->body = block();
        return node;
    }
    if (check("struct")) {
        auto node = std::make_shared<Stmt>();
        node->kind = Stmt::Kind::Struct; node->token = advance();
        node->name = expect("IDENT","expected structure name").value;
        expect("{"); separators();
        while (!check("}") && !check("EOF")) {
            if (check("var") || check("let")) advance();
            auto field = expect("IDENT","expected field name");
            expect(":");
            node->fields.push_back({field.value,typeName(),field});
            separators();
        }
        expect("}");
        return node;
    }
    if (check("let") || check("var") || check("const")) {
        auto node = std::make_shared<Stmt>();
        node->kind = Stmt::Kind::Variable; node->token = advance();
        node->mutable_value = node->token.kind == "var";
        node->name = expect("IDENT","expected variable name").value;
        if (match(":")) node->type = typeName();
        if (match("=")) node->value = expression();
        if (!node->mutable_value && !node->value)
            fail(node->token,"immutable declaration requires a value");
        return node;
    }
    if (check("import")) {
        auto node = std::make_shared<Stmt>();
        node->kind = Stmt::Kind::Import; node->token = advance();
        auto module = advance();
        if (module.kind != "IDENT" && module.kind != "STRING")
            fail(module,"expected module name");
        node->module = module.value;
        if (match("as")) node->alias = expect("IDENT").value;
        return node;
    }
    return statement();
}
StmtPtr Parser::statement() {
    if (check("if")) {
        auto node = std::make_shared<Stmt>();
        node->kind = Stmt::Kind::If; node->token = advance();
        node->condition = expression(); node->body = block();
        if (match("else")) {
            if (check("if")) node->alternate.push_back(statement());
            else node->alternate = block();
        }
        return node;
    }
    if (check("while")) {
        auto node = std::make_shared<Stmt>();
        node->kind = Stmt::Kind::While; node->token = advance();
        node->condition = expression(); node->body = block();
        return node;
    }
    if (check("return")) {
        auto node = std::make_shared<Stmt>();
        node->kind = Stmt::Kind::Return; node->token = advance();
        if (!check("}") && !check("NEWLINE") && !check(";") && !check("EOF"))
            node->value = expression();
        return node;
    }
    if (check("break") || check("continue") || check("pass") || check("log")) {
        auto node = std::make_shared<Stmt>();
        node->token = advance();
        if (node->token.kind == "break") node->kind = Stmt::Kind::Break;
        else if (node->token.kind == "continue") node->kind = Stmt::Kind::Continue;
        else if (node->token.kind == "log") {
            node->kind = Stmt::Kind::Log; node->value = expression();
        } else node->kind = Stmt::Kind::Pass;
        return node;
    }
    auto node = std::make_shared<Stmt>();
    node->kind = Stmt::Kind::Expression; node->value = expression();
    node->token = node->value->token;
    return node;
}
int Parser::precedence(std::string_view op) {
    if (op == "=") return 1;
    if (op == "or" || op == "||") return 2;
    if (op == "and" || op == "&&") return 3;
    if (op == "==" || op == "!=") return 4;
    if (op == "<" || op == "<=" || op == ">" || op == ">=") return 5;
    if (op == "+" || op == "-") return 6;
    if (op == "*" || op == "/" || op == "%") return 7;
    return 0;
}
ExprPtr Parser::expression(int minimum) {
    auto left = prefix();
    while (precedence(peek().kind) >= minimum) {
        auto op = advance();
        const int level = precedence(op.kind);
        auto right = expression(op.kind == "=" ? level : level + 1);
        auto node = std::make_shared<Expr>();
        node->kind = Expr::Kind::Binary; node->token = op; node->op = op.value;
        node->left = left; node->right = right; left = node;
    }
    return left;
}
ExprPtr Parser::prefix() {
    if (check("!") || check("-") || check("not")) {
        auto op = advance();
        auto node = std::make_shared<Expr>();
        node->kind = Expr::Kind::Unary; node->token = op; node->op = op.value;
        node->left = expression(8);
        return node;
    }
    auto token = advance();
    auto node = std::make_shared<Expr>(); node->token = token;
    if (token.kind == "INT" || token.kind == "FLOAT" || token.kind == "STRING" ||
        token.kind == "true" || token.kind == "false" || token.kind == "none") {
        node->kind = Expr::Kind::Literal; node->value = token.value;
        node->literal_kind = token.kind;
    } else if (token.kind == "IDENT") {
        node->kind = Expr::Kind::Name; node->value = token.value;
    } else if (token.kind == "(") {
        node = expression(); expect(")","expected ')' after expression");
    } else fail(token,"expected expression");

    while (true) {
        if (match("(")) {
            auto call = std::make_shared<Expr>();
            call->kind = Expr::Kind::Call; call->token = node->token; call->left = node;
            if (!check(")")) {
                do { call->arguments.push_back(expression()); } while (match(","));
            }
            expect(")","expected ')' after arguments");
            node = call;
        } else if (match(".")) {
            auto member = expect("IDENT","expected member name");
            auto access = std::make_shared<Expr>();
            access->kind = Expr::Kind::Member; access->token = member;
            access->left = node; access->value = member.value; node = access;
        } else break;
    }
    return node;
}
}
