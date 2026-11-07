#pragma once
#include "swan/Parse/Parser.hpp"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace swan {
inline StmtPtr Parser::declaration() {
    if (check("fn")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::Function; node->token = advance().location();
        node->name = std::move(expect("IDENT","expected function name").value);
        expect("(");
        if (!check(")")) {
            do {
                auto& parameter = expect("IDENT","expected parameter name");
                std::string type;
                if (match(":")) type = typeName();
                node->parameters.push_back({std::move(parameter.value),type,parameter.location()});
            } while (match(","));
        }
        expect(")");
        if (match("->")) node->return_type = typeName();
        node->body = block();
        return node;
    }
    if (check("struct")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::Struct; node->token = advance().location();
        node->name = std::move(expect("IDENT","expected structure name").value);
        expect("{"); separators();
        while (!check("}") && !check("EOF")) {
            if (check("var") || check("let")) advance();
            auto& field = expect("IDENT","expected field name");
            expect(":");
            node->fields.push_back({std::move(field.value),typeName(),field.location()});
            separators();
        }
        expect("}");
        return node;
    }
    if (check("let") || check("var") || check("const")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::Variable;
        const auto& keyword = advance();
        node->token = keyword.location();
        node->mutable_value = keyword.kind == "var";
        node->name = std::move(expect("IDENT","expected variable name").value);
        if (match(":")) node->type = typeName();
        if (match("=")) node->value = expression();
        if (!node->mutable_value && !node->value)
            fail(node->token,"immutable declaration requires a value");
        return node;
    }
    if (check("import")) {
        auto node = std::make_unique<Stmt>();
        node->kind = Stmt::Kind::Import; node->token = advance().location();
        auto& module = advance();
        if (module.kind != "IDENT" && module.kind != "STRING")
            fail(module,"expected module name");
        node->module = std::move(module.value);
        if (match("as")) node->alias = std::move(expect("IDENT").value);
        return node;
    }
    return statement();
}
}
