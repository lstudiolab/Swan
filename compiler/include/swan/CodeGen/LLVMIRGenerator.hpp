#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

namespace swan {

// Lowers the checked Swan AST directly to LLVM IR. No C source is produced.
class LLVMIRGenerator {
    struct Value { std::string type, value; };
    const Program& program_;
    std::string out_;
    std::string globals_;
    std::vector<std::string> allocas_;
    std::unordered_map<const Stmt*, std::string> slots_;
    std::vector<std::unordered_map<std::string, std::string>> scopes_;
    std::vector<std::pair<std::string, std::string>> loops_;
    unsigned temp_ = 0, label_ = 0, string_ = 0;
    bool terminated_ = false;
    std::string sizeType_ = "i64";
    std::unordered_map<std::string, std::string> strings_;

    static std::string ty(const std::string& t) {
        if (t == "int") return "i64";
        if (t == "float") return "double";
        if (t == "bool") return "i1";
        if (t == "string" || t == "int[]") return "ptr";
        if (t == "void" || t == "none") return "void";
        return "%swan." + t;
    }
    std::string tmp() { return "%t" + std::to_string(temp_++); }
    std::string block(const char* prefix) { return std::string(prefix) + std::to_string(label_++); }
    void line(const std::string& s) { out_ += "  " + s + "\n"; }
    void label(const std::string& s) { out_ += s + ":\n"; terminated_ = false; }
    void branch(const std::string& s) { line("br label %" + s); terminated_ = true; }
    static std::string bytes(const std::string& s) {
        std::string r;
        for (unsigned char c : s) {
            char b[5];
            if (c >= 32 && c <= 126 && c != '"' && c != '\\') r += static_cast<char>(c);
            else { std::snprintf(b, sizeof b, "\\%02X", c); r += b; }
        }
        return r;
    }
    Value stringValue(const std::string& s) {
        auto it = strings_.find(s);
        std::string name;
        if (it == strings_.end()) {
            name = "@.s" + std::to_string(string_++);
            strings_.emplace(s, name);
            std::string encoded = bytes(s);
            globals_ += name + " = private unnamed_addr constant [" +
                std::to_string(s.size() + 1) + " x i8] c\"" + encoded + "\\00\", align 1\n";
        } else name = it->second;
        auto p = tmp();
        line(p + " = getelementptr inbounds [" + std::to_string(s.size() + 1) +
             " x i8], ptr " + name + ", i64 0, i64 0");
        return {"ptr", p};
    }
    std::string lookup(const std::string& n) const {
        for (auto i = scopes_.rbegin(); i != scopes_.rend(); ++i) {
            auto f = i->find(n); if (f != i->end()) return f->second;
        }
        throw CompileError("unknown native variable '" + n + "'");
    }
    std::string address(const Expr* e) {
        if (e->kind == Expr::Kind::Name) return lookup(e->token.value);
        if (e->kind == Expr::Kind::Index) {
            auto base = emitExpr(e->left.get());
            auto index = emitExpr(e->right.get());
            auto p = tmp();
            line(p + " = getelementptr i64, ptr " + base.value + ", i64 " + index.value);
            return p;
        }
        if (e->kind == Expr::Kind::Member) {
            const auto structName = e->left->resolved_type;
            const Stmt* structure = nullptr;
            for (const auto& n : program_)
                if (n->kind == Stmt::Kind::Struct && n->name == structName) { structure = n.get(); break; }
            if (!structure) throw CompileError("unknown struct type '" + structName + "'");
            std::size_t index = 0;
            for (; index < structure->fields.size(); ++index)
                if (structure->fields[index].name == e->token.value) break;
            if (index == structure->fields.size()) throw CompileError("unknown struct field '" + e->token.value + "'");
            auto base = e->left->kind == Expr::Kind::Name ? lookup(e->left->token.value) : address(e->left.get()), p = tmp();
            line(p + " = getelementptr inbounds %swan." + structName + ", ptr " + base +
                 ", i32 0, i32 " + std::to_string(index));
            return p;
        }
        throw CompileError("unsupported assignment target in native IR backend");
    }
    Value cast(Value v, const std::string& wanted) {
        if (v.type == wanted) return v;
        if (v.type == "i64" && wanted == "double") {
            auto r = tmp(); line(r + " = sitofp i64 " + v.value + " to double"); return {wanted, r};
        }
        if (v.type == "i64" && wanted == "i32") { auto r = tmp(); line(r + " = trunc i64 " + v.value + " to i32"); return {wanted, r}; }
        if (v.type == "i1" && wanted == "i64") {
            auto r = tmp(); line(r + " = zext i1 " + v.value + " to i64"); return {wanted, r};
        }
        throw CompileError("unsupported native conversion from " + v.type + " to " + wanted);
    }
    Value emitExpr(const Expr* e) {
        const auto& v = e->token.value;
        switch (e->kind) {
        case Expr::Kind::Literal:
            if (e->token.kind == "STRING") return stringValue(v);
            if (e->token.kind == "true") return {"i1", "true"};
            if (e->token.kind == "false") return {"i1", "false"};
            if (e->token.kind == "none") return {"ptr", "null"};
            if (e->resolved_type == "float") return {"double", v};
            return {"i64", v};
        case Expr::Kind::Name: {
            auto p = lookup(v), t = ty(e->resolved_type), r = tmp();
            line(r + " = load " + t + ", ptr " + p);
            return {t, r};
        }
        case Expr::Kind::Unary: {
            auto a = emitExpr(e->left.get()); auto r = tmp();
            if (v == "-") line(r + " = " + std::string(a.type == "double" ? "fneg double " : "sub i64 0, ") + a.value);
            else if (v == "not" || v == "!") line(r + " = xor i1 " + a.value + ", true");
            else throw CompileError("unsupported unary operator '" + v + "'");
            return {a.type, r};
        }
        case Expr::Kind::Index: {
            auto p = address(e), r = tmp();
            line(r + " = load i64, ptr " + p);
            return {"i64", r};
        }
        case Expr::Kind::Member: {
            auto p = address(e), r = tmp();
            const auto structName = e->left->resolved_type;
            const Stmt* structure = nullptr;
            for (const auto& n : program_)
                if (n->kind == Stmt::Kind::Struct && n->name == structName) { structure = n.get(); break; }
            if (!structure) throw CompileError("unknown struct type '" + structName + "'");
            auto field = std::find_if(structure->fields.begin(), structure->fields.end(),
                [&](const Field& f) { return f.name == e->token.value; });
            if (field == structure->fields.end()) throw CompileError("unknown struct field '" + e->token.value + "'");
            auto t = ty(field->type);
            line(r + " = load " + t + ", ptr " + p);
            return {t, r};
        }
        case Expr::Kind::Call: {
            if (e->left->token.value == "array") {
                auto n = cast(emitExpr(e->arguments.at(0).get()), sizeType_);
                auto r = tmp(); line(r + " = call ptr @calloc(" + sizeType_ + " " + n.value + ", " + sizeType_ + " 8)");
                return {"ptr", r};
            }
            const Stmt* fn = nullptr;
            for (const auto& s : program_) if (s->kind == Stmt::Kind::Function && s->name == e->left->token.value) { fn = s.get(); break; }
            if (!fn) throw CompileError("unknown function '" + e->left->token.value + "'");
            std::string args;
            for (std::size_t i = 0; i < e->arguments.size(); ++i) {
                auto a = cast(emitExpr(e->arguments[i].get()), ty(fn->parameters.at(i).type));
                if (i) args += ", ";
                args += a.type + " " + a.value;
            }
            auto ret = fn->name == "main" ? "i32" : ty(fn->return_type);
            auto call = "call " + ret + " @" + (fn->name == "main" ? "main" : "swan_fn_" + fn->name) + "(" + args + ")";
            if (ret == "void") { line(call); return {"void", ""}; }
            auto r = tmp(); line(r + " = " + call); return {ret, r};
        }
        case Expr::Kind::Binary: {
            if (v == "=") {
                auto p = address(e->left.get()); auto a = cast(emitExpr(e->right.get()), ty(e->left->resolved_type));
                line("store " + a.type + " " + a.value + ", ptr " + p);
                return a;
            }
            auto a = emitExpr(e->left.get()), b = emitExpr(e->right.get());
            if (a.type != b.type) { if (a.type == "i64") a = cast(a, "double"); else b = cast(b, "double"); }
            auto r = tmp();
            const bool fp = a.type == "double";
            const std::string prefix = fp ? "f" : "";
            std::string op;
            if (v == "+") op = prefix + "add";
            else if (v == "-") op = prefix + "sub";
            else if (v == "*") op = prefix + "mul";
            else if (v == "/" || v == "//") op = fp ? "fdiv" : "sdiv";
            else if (v == "%") op = "srem";
            else if (v == "and") op = "and";
            else if (v == "or") op = "or";
            else if (v == "==") op = fp ? "fcmp oeq" : "icmp eq";
            else if (v == "!=") op = fp ? "fcmp one" : "icmp ne";
            else if (v == "<") op = fp ? "fcmp olt" : "icmp slt";
            else if (v == ">") op = fp ? "fcmp ogt" : "icmp sgt";
            else if (v == "<=") op = fp ? "fcmp ole" : "icmp sle";
            else if (v == ">=") op = fp ? "fcmp oge" : "icmp sge";
            else throw CompileError("unsupported binary operator '" + v + "'");
            line(r + " = " + op + " " + a.type + " " + a.value + ", " + b.value);
            const bool cmp = v == "==" || v == "!=" || v == "<" || v == ">" || v == "<=" || v == ">=";
            return {cmp ? "i1" : a.type, r};
        }
        default: throw CompileError("unsupported expression in native IR backend");
        }
    }
    void collect(const std::vector<StmtPtr>& body) {
        for (const auto& s : body) {
            if (s->kind == Stmt::Kind::Variable) {
                auto slot = "%v" + std::to_string(slots_.size());
                slots_[s.get()] = slot;
                allocas_.push_back(slot + " = alloca " + ty(s->type.empty() ? s->value->resolved_type : s->type));
            }
            collect(s->body); collect(s->alternate);
        }
    }
    bool emitBlock(const std::vector<StmtPtr>& body) {
        scopes_.emplace_back();
        for (const auto& s : body) {
            if (terminated_) break;
            if (s->kind == Stmt::Kind::Variable) {
                auto p = slots_.at(s.get()); scopes_.back()[s->name] = p;
                auto t = ty(s->type.empty() ? (s->value ? s->value->resolved_type : "int") : s->type);
                auto a = s->value ? cast(emitExpr(s->value.get()), t) : Value{t, t == "ptr" ? "null" : t == "double" ? "0.0" : (!t.empty() && t[0] == '%') ? "zeroinitializer" : "0"};
                line("store " + t + " " + a.value + ", ptr " + p);
            } else if (s->kind == Stmt::Kind::Log) {
                auto a = emitExpr(s->value.get());
                if (a.type == "ptr") line("call i32 @puts(ptr " + a.value + ")");
                else {
                    std::string fmt = a.type == "double" ? "%g\n" : a.type == "i1" ? "%d\n" : "%lld\n";
                    auto f = stringValue(fmt);
                    if (a.type == "i1") { auto z = tmp(); line(z + " = zext i1 " + a.value + " to i32"); a = {"i32", z}; }
                    line("call i32 (ptr, ...) @printf(ptr " + f.value + ", " + a.type + " " + a.value + ")");
                }
            } else if (s->kind == Stmt::Kind::Return) {
                if (s->value) {
                    auto a = emitExpr(s->value.get());
                    if (s->token.value == "main") {}
                    if (a.type == "i64" && currentReturn_ == "i32") a = cast(a, "i32");
                    line("ret " + currentReturn_ + " " + a.value);
                } else line("ret void");
                terminated_ = true;
            } else if (s->kind == Stmt::Kind::Expression) emitExpr(s->value.get());
            else if (s->kind == Stmt::Kind::Pass || s->kind == Stmt::Kind::Import) {}
            else if (s->kind == Stmt::Kind::Break || s->kind == Stmt::Kind::Continue) {
                if (loops_.empty()) throw CompileError("loop control outside loop");
                branch(s->kind == Stmt::Kind::Break ? loops_.back().first : loops_.back().second);
            } else if (s->kind == Stmt::Kind::If) {
                auto c = emitExpr(s->condition.get());
                if (c.type != "i1") { auto z = tmp(); line(z + " = icmp ne " + c.type + " " + c.value + ", 0"); c = {"i1", z}; }
                auto yes = block("if.then."), no = block("if.else."), end = block("if.end.");
                line("br i1 " + c.value + ", label %" + yes + ", label %" + no); terminated_ = true;
                label(yes); bool yt = emitBlock(s->body); if (!yt) branch(end);
                label(no); bool nt = false; if (!s->alternate.empty()) nt = emitBlock(s->alternate); if (!nt) branch(end);
                label(end);
                if (yt && nt) { line("unreachable"); terminated_ = true; }
            } else if (s->kind == Stmt::Kind::While) {
                auto cond = block("while.cond."), bodyLabel = block("while.body."), done = block("while.end.");
                branch(cond); label(cond);
                auto c = emitExpr(s->condition.get());
                if (c.type != "i1") { auto z = tmp(); line(z + " = icmp ne " + c.type + " " + c.value + ", 0"); c = {"i1", z}; }
                line("br i1 " + c.value + ", label %" + bodyLabel + ", label %" + done); terminated_ = true;
                label(bodyLabel); loops_.push_back({done, cond}); bool bt = emitBlock(s->body); loops_.pop_back();
                if (!bt) branch(cond);
                label(done);
            } else throw CompileError("unsupported statement in native IR backend");
        }
        bool done = terminated_;
        scopes_.pop_back();
        return done;
    }
    void validateStructs() const {
        std::unordered_map<std::string, const Stmt*> structures;
        std::unordered_map<std::string, unsigned char> marks;
        for (const auto& node : program_)
            if (node->kind == Stmt::Kind::Struct) structures.emplace(node->name, node.get());
        const auto visit = [&](const auto& self, const Stmt* structure) -> void {
            auto& mark = marks[structure->name];
            if (mark == 2) return;
            if (mark == 1) throw CompileError("struct value cycle involving '" + structure->name + "'");
            mark = 1;
            for (const auto& field : structure->fields) {
                auto found = structures.find(field.type);
                if (found != structures.end()) self(self, found->second);
            }
            mark = 2;
        };
        for (const auto& entry : structures) visit(visit, entry.second);
    }
    std::string currentReturn_;
public:
    explicit LLVMIRGenerator(const Program& p, bool size32 = false) : program_(p), sizeType_(size32 ? "i32" : "i64") {}
    std::string generate() {
        validateStructs();
        out_ = "source_filename = \"swan\"\n";
        for (const auto& s : program_) if (s->kind == Stmt::Kind::Struct) {
            out_ += "%swan." + s->name + " = type { ";
            for (std::size_t i = 0; i < s->fields.size(); ++i) { if (i) out_ += ", "; out_ += ty(s->fields[i].type); }
            out_ += " }\n";
        }
        out_ += "declare i32 @printf(ptr, ...)\ndeclare i32 @puts(ptr)\ndeclare ptr @calloc(i64, i64)\n";
        for (const auto& s : program_) if (s->kind == Stmt::Kind::Function) {
            temp_ = label_ = 0; slots_.clear(); allocas_.clear(); scopes_.clear(); loops_.clear(); terminated_ = false;
            currentReturn_ = s->name == "main" ? "i32" : ty(s->return_type);
            std::vector<std::string> params;
            for (std::size_t i = 0; i < s->parameters.size(); ++i) params.push_back("%arg" + std::to_string(i));
            collect(s->body);
            std::string head = "define " + currentReturn_ + " @" + (s->name == "main" ? "main" : "swan_fn_" + s->name) + "(";
            for (std::size_t i = 0; i < s->parameters.size(); ++i) { if (i) head += ", "; head += ty(s->parameters[i].type) + " " + params[i]; }
            out_ += head + ") {\nentry:\n";
            for (const auto& a : allocas_) out_ += "  " + a + "\n";
            scopes_.emplace_back();
            for (std::size_t i = 0; i < s->parameters.size(); ++i) {
                auto p = "%p" + std::to_string(i); auto t = ty(s->parameters[i].type);
                out_ += "  " + p + " = alloca " + t + "\n  store " + t + " " + params[i] + ", ptr " + p + "\n";
                scopes_.back()[s->parameters[i].name] = p;
            }
            bool done = emitBlock(s->body);
            if (!done) out_ += "  ret " + currentReturn_ + (currentReturn_ == "void" ? "" : " 0") + "\n";
            out_ += "}\n";
        }
        return out_ + globals_;
    }
};

} // namespace swan
