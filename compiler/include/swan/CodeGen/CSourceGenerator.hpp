#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <string>
#include <vector>

namespace swan {

class CSourceGenerator {
public:
    explicit CSourceGenerator(const Program& program) : program_(program) {}

    std::string generate() const {
        std::string out =
            "typedef long long swan_int;\n"
            "typedef int swan_bool;\n"
            "extern int printf(const char *, ...);\n"
            "extern int puts(const char *);\n"
            "extern void *calloc(unsigned long, unsigned long);\n\n";
        for (const auto& node : program_)
            if (node->kind == Stmt::Kind::Struct) emitStruct(out, node.get());
        for (const auto& node : program_)
            if (node->kind == Stmt::Kind::Function) emitPrototype(out, node.get());
        out += "\n";
        for (const auto& node : program_)
            if (node->kind == Stmt::Kind::Function) emitFunction(out, node.get());
        return out;
    }

private:
    const Program& program_;

    static std::string type(const std::string& value) {
        if (value == "int") return "swan_int";
        if (value == "float") return "double";
        if (value == "bool") return "swan_bool";
        if (value == "string") return "const char *";
        if (value == "int[]") return "swan_int *";
        if (value == "void" || value == "none") return "void";
        return "struct " + value;
    }

    static std::string functionName(const std::string& name) {
        return name == "main" ? "main" : "swan_fn_" + name;
    }

    static std::string functionType(const Stmt* node) {
        return node->name == "main" ? "int" : type(node->return_type);
    }

    static std::string quote(const std::string& value) {
        std::string result = "\"";
        for (unsigned char ch : value) {
            if (ch == '"' || ch == '\\') { result.push_back('\\'); result.push_back(static_cast<char>(ch)); }
            else if (ch == '\n') result += "\\n";
            else if (ch == '\r') result += "\\r";
            else if (ch == '\t') result += "\\t";
            else if (ch < 32 || ch >= 127) {
                const char digits[] = "01234567";
                result += "\\";
                result.push_back(digits[(ch >> 6) & 7]);
                result.push_back(digits[(ch >> 3) & 7]);
                result.push_back(digits[ch & 7]);
            } else result.push_back(static_cast<char>(ch));
        }
        result.push_back('"');
        return result;
    }

    void emitStruct(std::string& out, const Stmt* node) const {
        out += "struct " + node->name + " {\n";
        for (const auto& field : node->fields)
            out += "    " + type(field.type) + " " + field.name + ";\n";
        out += "};\n";
    }

    void emitPrototype(std::string& out, const Stmt* node) const {
        out += functionType(node) + " " + functionName(node->name) + "(";
        if (node->parameters.empty()) out += "void";
        for (std::size_t i = 0; i < node->parameters.size(); ++i) {
            if (i) out += ", ";
            out += type(node->parameters[i].type) + " " + node->parameters[i].name;
        }
        out += ");\n";
    }

    std::string expression(const Expr* node) const {
        const auto& value = node->token.value;
        switch (node->kind) {
        case Expr::Kind::Literal:
            if (node->token.kind == "STRING") return quote(value);
            if (value == "true") return "1";
            if (value == "false" || value == "none") return "0";
            return value;
        case Expr::Kind::Name:
            return value;
        case Expr::Kind::Unary:
            return "(" + std::string(value == "not" ? "!" : value) +
                expression(node->left.get()) + ")";
        case Expr::Kind::Index:
            return "(" + expression(node->left.get()) + "[" +
                expression(node->right.get()) + "])";
        case Expr::Kind::Member:
            return "(" + expression(node->left.get()) + "." + value + ")";
        case Expr::Kind::Call: {
            if (node->left->token.value == "array")
                return "((swan_int *)calloc((unsigned long)(" +
                    expression(node->arguments[0].get()) + "), sizeof(swan_int)))";
            std::string result = functionName(node->left->token.value) + "(";
            for (std::size_t i = 0; i < node->arguments.size(); ++i) {
                if (i) result += ", ";
                result += expression(node->arguments[i].get());
            }
            return result + ")";
        }
        case Expr::Kind::Binary: {
            const auto& op = node->token.value;
            const std::string left = expression(node->left.get());
            const std::string right = expression(node->right.get());
            if (op == "//") return "((" + left + ") / (" + right + "))";
            if (op == "and") return "((" + left + ") && (" + right + "))";
            if (op == "or") return "((" + left + ") || (" + right + "))";
            if (node->resolved_type == "float" &&
                (op == "+" || op == "-" || op == "*" || op == "/"))
                return "(((double)(" + left + ")) " + op + " ((double)(" + right + ")))";
            return "((" + left + ") " + op + " (" + right + "))";
        }
        }
        throw CompileError("unsupported expression in portable code generator");
    }

    void emitStatement(std::string& out, const Stmt* node, unsigned indent) const {
        const std::string pad(indent * 4, ' ');
        switch (node->kind) {
        case Stmt::Kind::Variable: {
            const std::string declared = node->type.empty()
                ? node->value->resolved_type : node->type;
            out += pad + type(declared) + " " + node->name;
            if (node->value) out += " = " + expression(node->value.get());
            else out += " = 0";
            out += ";\n";
            break;
        }
        case Stmt::Kind::Log: {
            const auto valueType = node->value->resolved_type;
            if (valueType == "string") out += pad + "puts(" + expression(node->value.get()) + ");\n";
            else if (valueType == "float") out += pad + "printf(\"%g\\n\", " + expression(node->value.get()) + ");\n";
            else if (valueType == "bool") out += pad + "printf(\"%d\\n\", " + expression(node->value.get()) + ");\n";
            else out += pad + "printf(\"%lld\\n\", " + expression(node->value.get()) + ");\n";
            break;
        }
        case Stmt::Kind::Return:
            out += pad + "return";
            if (node->value) out += " " + expression(node->value.get());
            out += ";\n";
            break;
        case Stmt::Kind::Expression:
            out += pad + expression(node->value.get()) + ";\n";
            break;
        case Stmt::Kind::If:
            out += pad + "if (" + expression(node->condition.get()) + ") {\n";
            emitBlock(out, node->body, indent + 1);
            out += pad + "}";
            if (!node->alternate.empty()) {
                out += " else {\n";
                emitBlock(out, node->alternate, indent + 1);
                out += pad + "}";
            }
            out += "\n";
            break;
        case Stmt::Kind::While:
            out += pad + "while (" + expression(node->condition.get()) + ") {\n";
            emitBlock(out, node->body, indent + 1);
            out += pad + "}\n";
            break;
        case Stmt::Kind::Break: out += pad + "break;\n"; break;
        case Stmt::Kind::Continue: out += pad + "continue;\n"; break;
        case Stmt::Kind::Pass: out += pad + ";\n"; break;
        case Stmt::Kind::Import: break;
        default:
            throw CompileError("unsupported statement in portable code generator");
        }
    }

    void emitBlock(std::string& out, const std::vector<StmtPtr>& body, unsigned indent) const {
        for (const auto& node : body) emitStatement(out, node.get(), indent);
    }

    void emitFunction(std::string& out, const Stmt* node) const {
        out += functionType(node) + " " + functionName(node->name) + "(";
        if (node->parameters.empty()) out += "void";
        for (std::size_t i = 0; i < node->parameters.size(); ++i) {
            if (i) out += ", ";
            out += type(node->parameters[i].type) + " " + node->parameters[i].name;
        }
        out += ") {\n";
        emitBlock(out, node->body, 1);
        if (node->return_type == "void") out += "    return;\n";
        out += "}\n\n";
    }
};

} // namespace swan
