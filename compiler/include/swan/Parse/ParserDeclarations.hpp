#pragma once
#include "swan/Parse/Parser.hpp"
#include <memory>
#include <string>
#include <utility>
namespace swan {
inline StmtPtr Parser::declaration() {
    if (check("fn")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::Function; node->token = advance();
        node->name = expect("IDENT","expected function name").value;
        expect("(");
        if (!check(")")) {
            do {
                const auto& parameter = expect("IDENT","expected parameter name");
                std::string type;
                if (match(":")) type = typeName();
                node->parameters.push_back({parameter.value,type,parameter});
            } while (match(","));
        }
        expect(")");
        if (match("->")) node->return_type = typeName();
        expect(":","expected ':' after function declaration");
        node->body = block();
        if (node->return_type == "void") {
            const auto has_value_return = [&](const auto& self, const std::vector<StmtPtr>& body) -> bool {
                for (const auto& item : body) {
                    if (item->kind == Stmt::Kind::Return && item->value) return true;
                    if (self(self, item->body) || self(self, item->alternate)) return true;
                }
                return false;
            };
            if (has_value_return(has_value_return, node->body)) node->return_type = "int";
        }
        return node;
    }
    if (check("struct")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::Struct; node->token = advance();
        node->name = expect("IDENT","expected structure name").value;
        expect(":","expected ':' after structure name");
        separators();
        expect("INDENT","expected indentation for structure fields");
        while (!check("DEDENT") && !check("EOF")) {
            if (check("var") || check("let")) advance();
            const auto& field = expect("IDENT","expected field name");
            expect(":");
            node->fields.push_back({field.value,typeName(),field});
            separators();
        }
        expect("DEDENT","expected indentation to close structure");
        return node;
    }
    if (check("let") || check("var") || check("const")) {
        auto node = std::make_unique<Stmt>();
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
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::Import; node->token = advance();
        const auto& module = advance();
        if (module.kind != "IDENT" && module.kind != "STRING")
            fail(module,"expected module name");
        node->module = module.value;
        if (match("as")) node->alias = expect("IDENT").value;
        return node;
    }
    return statement();
}
}
