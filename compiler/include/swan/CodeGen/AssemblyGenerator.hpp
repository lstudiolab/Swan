#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace swan {

// Direct GNU assembler backend for x86-64 System V ELF targets.
// Swan's internal calling convention passes every scalar argument in a
// general-purpose register; external libc calls follow the platform ABI.
class AssemblyGenerator {
public:
    explicit AssemblyGenerator(const Program& program) : program_(program) {}

    std::string generate() {
#if !defined(__linux__) || !defined(__x86_64__)
        throw CompileError("the assembly backend currently targets x86-64 Linux (System V ELF)");
#endif
        text_.clear();
        data_.clear();
        strings_.clear();
        floats_.clear();
        next_label_ = 0;
        validateStructs();
        data_ = ".section .rodata\n"
                ".Lfmt_int: .asciz \"%lld\\n\"\n"
                ".Lfmt_float: .asciz \"%g\\n\"\n"
                ".Lfmt_bool: .asciz \"%d\\n\"\n"
                ".Lfloat_sign: .quad 0x8000000000000000, 0\n";

        text_ += ".intel_syntax noprefix\n"
                 ".text\n"
                 ".extern printf\n"
                 ".extern puts\n"
                 ".extern calloc\n";

        for (const auto& node : program_) {
            if (node->kind == Stmt::Kind::Function) emitFunction(node.get());
        }

        return data_ + text_ + ".section .note.GNU-stack,\"\",@progbits\n";
    }

private:
    struct Location {
        std::string reg;
        int offset = 0;
        bool in_register = false;
    };
    using Scope = std::unordered_map<std::string, Location>;
    static inline constexpr const char* local_registers_[] = {
        "rbx", "r12", "r13", "r14", "r15", "r8", "r9"
    };
    static inline constexpr const char* argument_registers_[] = {
        "rdi", "rsi", "rdx", "rcx", "r8", "r9"
    };

    const Program& program_;
    std::string text_;
    std::string data_;
    std::unordered_map<std::string, std::string> strings_;
    std::unordered_map<std::string, std::string> floats_;
    std::unordered_map<const Stmt*, Location> statement_locations_;
    std::vector<Scope> scopes_;
    std::vector<std::pair<std::string, std::string>> loops_;
    int next_spill_ = 0;
    unsigned next_register_ = 0;
    int frame_size_ = 0;
    int stack_bytes_ = 0;
    unsigned next_label_ = 0;
    std::string epilogue_;

    void validateStructs() const {
        std::unordered_map<std::string_view, const Stmt*> structures;
        std::unordered_map<std::string_view, unsigned char> marks;
        structures.reserve(program_.size());
        marks.reserve(program_.size());
        for (const auto& node : program_)
            if (node->kind == Stmt::Kind::Struct)
                structures.emplace(node->name, node.get());
        const auto visit = [&](const auto& self, const Stmt* structure) -> void {
            auto& mark = marks[structure->name];
            if (mark == 2) return;
            if (mark == 1)
                throw CompileError("struct value cycle involving '" + structure->name + "'");
            mark = 1;
            for (const auto& field : structure->fields) {
                const auto dependency = structures.find(field.type);
                if (dependency != structures.end()) self(self, dependency->second);
            }
            mark = 2;
        };
        for (const auto& entry : structures) visit(visit, entry.second);
    }

    std::string label(const char* prefix) {
        return std::string(".L") + prefix + std::to_string(next_label_++);
    }

    void emit(const std::string& value) { text_ += value; text_.push_back('\n'); }

    Location allocateLocation() {
        if (next_register_ < sizeof(local_registers_) / sizeof(local_registers_[0]))
            return {local_registers_[next_register_++], 0, true};
        return {{}, 48 + next_spill_++ * 8, false};
    }

    Location lookupLocation(std::string_view name) const {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(std::string(name));
            if (found != scope->end()) return found->second;
        }
        throw CompileError("internal error: missing native location for '" + std::string(name) + "'");
    }

    void collectBlock(const std::vector<StmtPtr>& statements) {
        scopes_.emplace_back();
        for (const auto& node : statements) {
            if (node->kind == Stmt::Kind::Variable) {
                const auto location = allocateLocation();
                scopes_.back().emplace(node->name, location);
                statement_locations_.emplace(node.get(), location);
            } else if (node->kind == Stmt::Kind::If) {
                collectBlock(node->body);
                if (!node->alternate.empty()) collectBlock(node->alternate);
            } else if (node->kind == Stmt::Kind::While) {
                collectBlock(node->body);
            }
        }
        scopes_.pop_back();
    }

    std::string internString(const std::string& value) {
        if (const auto found = strings_.find(value); found != strings_.end())
            return found->second;
        const auto name = ".Lstr" + std::to_string(strings_.size());
        strings_.emplace(value, name);
        data_ += name + ":\n";
        if (value.empty()) {
            data_ += "    .byte 0\n";
        } else {
            data_ += "    .byte ";
            for (std::size_t i = 0; i < value.size(); ++i) {
                if (i) data_ += ", ";
                data_ += std::to_string(static_cast<unsigned char>(value[i]));
            }
            data_ += ", 0\n";
        }
        return name;
    }

    std::string internFloat(const std::string& value) {
        if (const auto found = floats_.find(value); found != floats_.end())
            return found->second;
        const auto name = ".Ldouble" + std::to_string(floats_.size());
        floats_.emplace(value, name);
        data_ += name + ": .double " + value + "\n";
        return name;
    }

    void pushRax() {
        emit("    push rax");
        stack_bytes_ += 8;
    }

    void popRcx() {
        emit("    pop rcx");
        stack_bytes_ -= 8;
    }

    void pushCallerSavedLocals() {
        emit("    push r8");
        emit("    push r9");
        stack_bytes_ += 16;
    }

    void popCallerSavedLocals() {
        emit("    pop r9");
        emit("    pop r8");
        stack_bytes_ -= 16;
    }

    void emitArrayAddress(const Expr* node) {
        const auto* index = node->right.get();
        if (index->kind == Expr::Kind::Name || index->kind == Expr::Kind::Literal) {
            emitExpression(node->left.get());
            emit("    mov r10, rax");
            emitExpression(index);
            emit("    lea rax, [r10 + rax*8]");
            return;
        }
        emitExpression(node->left.get());
        pushRax();
        emitExpression(index);
        popRcx();
        emit("    lea rax, [rcx + rax*8]");
    }

    void callAligned(const std::string& target) {
        const bool pad = (stack_bytes_ & 15) != 0;
        if (pad) {
            emit("    sub rsp, 8");
            stack_bytes_ += 8;
        }
        emit("    call " + target);
        if (pad) {
            emit("    add rsp, 8");
            stack_bytes_ -= 8;
        }
    }

    void emitFunction(const Stmt* function) {
        next_spill_ = 0;
        next_register_ = 0;
        stack_bytes_ = 0;
        statement_locations_.clear();
        scopes_.clear();
        scopes_.emplace_back();

        std::vector<Location> parameter_locations;
        parameter_locations.reserve(function->parameters.size());
        for (const auto& parameter : function->parameters) {
            const auto location = allocateLocation();
            scopes_.back().emplace(parameter.name, location);
            parameter_locations.push_back(location);
        }
        collectBlock(function->body);
        const int required = 40 + next_spill_ * 8;
        frame_size_ = ((required + 7) / 16) * 16 + 8;
        epilogue_ = label("return");

        const std::string name = function->name == "main"
            ? "main" : "swan_fn_" + function->name;
        emit(".globl " + name);
        emit(".type " + name + ", @function");
        emit(name + ":");
        emit("    push rbp");
        emit("    mov rbp, rsp");
        emit("    push rbx");
        emit("    push r12");
        emit("    push r13");
        emit("    push r14");
        emit("    push r15");
        if (frame_size_) emit("    sub rsp, " + std::to_string(frame_size_));


        for (std::size_t i = 0; i < parameter_locations.size(); ++i) {
            if (i >= 6) fail(function->parameters[i].token,
                "the native backend currently supports at most six parameters");
            const auto& location = parameter_locations[i];
            if (location.in_register)
                emit("    mov " + location.reg + ", " + argument_registers_[i]);
            else
                emit("    mov QWORD PTR [rbp - " + std::to_string(location.offset) +
                     "], " + argument_registers_[i]);
        }

        scopes_.clear();
        scopes_.emplace_back();
        for (std::size_t i = 0; i < function->parameters.size(); ++i)
            scopes_.back().emplace(function->parameters[i].name, parameter_locations[i]);

        emitBlock(function->body);
        if (function->return_type == "float") emit("    pxor xmm0, xmm0");
        else emit("    xor eax, eax");
        emit(epilogue_ + ":");
        emit("    lea rsp, [rbp - 40]");
        emit("    pop r15");
        emit("    pop r14");
        emit("    pop r13");
        emit("    pop r12");
        emit("    pop rbx");
        emit("    pop rbp");
        emit("    ret");
        emit(".size " + name + ", .-" + name);
        emit("");
        scopes_.clear();
    }

    void emitConditionFalse(const Expr* condition, const std::string& target) {
        if (condition->kind == Expr::Kind::Binary) {
            const auto& op = condition->token.value;
            const bool comparison = op == "==" || op == "!=" || op == "<" ||
                op == ">" || op == "<=" || op == ">=";
            const auto* right = condition->right.get();
            if (comparison && condition->left->resolved_type == "int" &&
                right->resolved_type == "int" &&
                (right->kind == Expr::Kind::Name || right->kind == Expr::Kind::Literal)) {
                emitExpression(condition->left.get());
                emit("    mov r10, rax");
                emitExpression(right);
                emit("    cmp r10, rax");
                const char* inverse = op == "==" ? "jne" : op == "!=" ? "je" :
                    op == "<" ? "jge" : op == ">" ? "jle" :
                    op == "<=" ? "jg" : "jl";
                emit(std::string("    ") + inverse + " " + target);
                return;
            }
        }
        emitExpression(condition);
        emit("    test rax, rax");
        emit("    jz " + target);
    }

    void emitBlock(const std::vector<StmtPtr>& statements) {
        scopes_.emplace_back();
        for (const auto& node : statements) emitStatement(node.get());
        scopes_.pop_back();
    }

    void emitStatement(const Stmt* node) {
        switch (node->kind) {
        case Stmt::Kind::Variable: {
            const auto location = statement_locations_.at(node);
            scopes_.back().emplace(node->name, location);
            if (node->value) {
                emitExpression(node->value.get());
                if (node->type == "float" && node->value->resolved_type == "int")
                    emit("    cvtsi2sd xmm0, rax");
                const bool floating = node->type == "float" ||
                    (node->type.empty() && node->value->resolved_type == "float");
                if (location.in_register) {
                    if (floating) emit("    movq " + location.reg + ", xmm0");
                    else emit("    mov " + location.reg + ", rax");
                } else if (floating) {
                    emit("    movsd QWORD PTR [rbp - " + std::to_string(location.offset) + "], xmm0");
                } else {
                    emit("    mov QWORD PTR [rbp - " + std::to_string(location.offset) + "], rax");
                }
            } else {
                if (location.in_register) emit("    xor " + location.reg + ", " + location.reg);
                else emit("    mov QWORD PTR [rbp - " + std::to_string(location.offset) + "], 0");
            }
            break;
        }
        case Stmt::Kind::Log: {
            const auto type = node->value->resolved_type;
            emitExpression(node->value.get());
            pushCallerSavedLocals();
            if (type == "string") {
                emit("    mov rdi, rax");
                callAligned("puts@PLT");
            } else if (type == "float") {
                emit("    lea rdi, [rip + .Lfmt_float]");
                emit("    mov eax, 1");
                callAligned("printf@PLT");
            } else if (type == "bool") {
                emit("    lea rdi, [rip + .Lfmt_bool]");
                emit("    mov rsi, rax");
                emit("    xor eax, eax");
                callAligned("printf@PLT");
            } else if (type == "int") {
                emit("    lea rdi, [rip + .Lfmt_int]");
                emit("    mov rsi, rax");
                emit("    xor eax, eax");
                callAligned("printf@PLT");
            } else {
                fail(node->token, "assembly backend cannot print this type");
            }
            popCallerSavedLocals();
            break;
        }
        case Stmt::Kind::Return:
            if (node->value) emitExpression(node->value.get());
            else emit("    xor eax, eax");
            emit("    jmp " + epilogue_);
            break;
        case Stmt::Kind::Expression:
            emitExpression(node->value.get());
            break;
        case Stmt::Kind::If: {
            const auto otherwise = label("else");
            const auto done = label("endif");
            emitConditionFalse(node->condition.get(), otherwise);
            emitBlock(node->body);
            emit("    jmp " + done);
            emit(otherwise + ":");
            if (!node->alternate.empty()) emitBlock(node->alternate);
            emit(done + ":");
            break;
        }
        case Stmt::Kind::While: {
            const auto condition = label("while");
            const auto done = label("endwhile");
            emit(condition + ":");
            emitConditionFalse(node->condition.get(), done);
            loops_.emplace_back(done, condition);
            emitBlock(node->body);
            loops_.pop_back();
            emit("    jmp " + condition);
            emit(done + ":");
            break;
        }
        case Stmt::Kind::Break:
            if (loops_.empty()) fail(node->token, "break outside loop");
            emit("    jmp " + loops_.back().first);
            break;
        case Stmt::Kind::Continue:
            if (loops_.empty()) fail(node->token, "continue outside loop");
            emit("    jmp " + loops_.back().second);
            break;
        case Stmt::Kind::Pass:
            emit("    nop");
            break;
        case Stmt::Kind::Struct:
            break;
        default:
            fail(node->token, "unsupported statement in assembly backend");
        }
    }

    void emitExpression(const Expr* node) {
        const auto& value = node->token.value;
        switch (node->kind) {
        case Expr::Kind::Literal:
            if (node->token.kind == "STRING") {
                emit("    lea rax, [rip + " + internString(value) + "]");
            } else if (node->token.kind == "FLOAT") {
                emit("    movsd xmm0, QWORD PTR [rip + " + internFloat(value) + "]");
            } else if (node->token.kind == "true") {
                emit("    mov eax, 1");
            } else if (node->token.kind == "false" || node->token.kind == "none") {
                emit("    xor eax, eax");
            } else {
                emit("    movabs rax, " + value);
            }
            return;
        case Expr::Kind::Name: {
            const auto location = lookupLocation(value);
            if (location.in_register) {
                if (node->resolved_type == "float") emit("    movq xmm0, " + location.reg);
                else emit("    mov rax, " + location.reg);
            } else if (node->resolved_type == "float") {
                emit("    movsd xmm0, QWORD PTR [rbp - " + std::to_string(location.offset) + "]");
            } else {
                emit("    mov rax, QWORD PTR [rbp - " + std::to_string(location.offset) + "]");
            }
            return;
        }
        case Expr::Kind::Unary:
            emitExpression(node->left.get());
            if (value == "not" || value == "!") {
                emit("    test rax, rax");
                emit("    sete al");
                emit("    movzx rax, al");
            } else if (node->resolved_type == "float") {
                emit("    xorpd xmm0, XMMWORD PTR [rip + .Lfloat_sign]");
            } else {
                emit("    neg rax");
            }
            return;
        case Expr::Kind::Binary:
            emitBinary(node);
            return;
        case Expr::Kind::Call:
            emitCall(node);
            return;
        case Expr::Kind::Index:
            emitArrayAddress(node);
            emit("    mov rax, QWORD PTR [rax]");
            return;
        case Expr::Kind::Member:
            fail(node->token, "member access is not implemented by the native backend");
        }
    }

    void emitBinary(const Expr* node) {
        const auto& op = node->token.value;
        if (op == "%" && node->right->kind == Expr::Kind::Literal &&
            node->right->token.kind == "INT" && node->right->token.value == "97") {
            emitExpression(node->left.get());
            emitModulo97();
            return;
        }
        if (op == "//" && node->right->kind == Expr::Kind::Literal &&
            node->right->token.kind == "INT") {
            const auto divisor = std::stoull(node->right->token.value);
            if (divisor != 0 && (divisor & (divisor - 1)) == 0) {
                emitExpression(node->left.get());
                if (divisor > 1) {
                    unsigned shift = 0;
                    for (auto value = divisor; value > 1; value >>= 1) ++shift;
                    emit("    mov r11, rax");
                    emit("    sar r11, 63");
                    emit("    and r11, " + std::to_string(divisor - 1));
                    emit("    add rax, r11");
                    emit("    sar rax, " + std::to_string(shift));
                }
                return;
            }
        }
        if (op == "=") {
            if (node->left->kind == Expr::Kind::Index) {
                emitArrayAddress(node->left.get());
                pushRax();
                emitExpression(node->right.get());
                popRcx();
                emit("    mov QWORD PTR [rcx], rax");
                return;
            }
            emitExpression(node->right.get());
            const auto location = lookupLocation(node->left->token.value);
            if (node->left->resolved_type == "float" &&
                node->right->resolved_type == "int")
                emit("    cvtsi2sd xmm0, rax");
            if (location.in_register) {
                if (node->left->resolved_type == "float") emit("    movq " + location.reg + ", xmm0");
                else emit("    mov " + location.reg + ", rax");
            } else if (node->left->resolved_type == "float") {
                emit("    movsd QWORD PTR [rbp - " + std::to_string(location.offset) + "], xmm0");
            } else {
                emit("    mov QWORD PTR [rbp - " + std::to_string(location.offset) + "], rax");
            }
            return;
        }
        if (op == "and" || op == "&&" || op == "or" || op == "||") {
            const auto done = label("logic");
            emitExpression(node->left.get());
            emit("    test rax, rax");
            emit(std::string("    ") + ((op == "and" || op == "&&") ? "jz " : "jnz ") + done);
            emitExpression(node->right.get());
            emit("    test rax, rax");
            emit("    setne al");
            emit("    movzx rax, al");
            emit("    jmp " + done + "_end");
            emit(done + ":");
            emit(std::string("    mov eax, ") + ((op == "and" || op == "&&") ? "0" : "1"));
            emit(done + "_end:");
            return;
        }

        const bool integerFastPath = node->left->resolved_type == "int" &&
            node->right->resolved_type == "int" &&
            (node->right->kind == Expr::Kind::Name || node->right->kind == Expr::Kind::Literal) &&
            (op == "+" || op == "-" || op == "*" || op == "==" || op == "!=" ||
             op == "<" || op == ">" || op == "<=" || op == ">=");
        if (integerFastPath) {
            if (node->right->kind == Expr::Kind::Literal &&
                node->right->token.value == "1" && (op == "+" || op == "-")) {
                emitExpression(node->left.get());
                emit(op == "+" ? "    inc rax" : "    dec rax");
                return;
            }
            emitExpression(node->left.get());
            emit("    mov r10, rax");
            emitExpression(node->right.get());
            if (op == "+") emit("    add rax, r10");
            else if (op == "-") {
                emit("    sub r10, rax");
                emit("    mov rax, r10");
            } else if (op == "*") emit("    imul rax, r10");
            else {
                emit("    cmp r10, rax");
                const char* condition = op == "==" ? "sete" : op == "!=" ? "setne" :
                    op == "<" ? "setl" : op == ">" ? "setg" :
                    op == "<=" ? "setle" : "setge";
                emit(std::string("    ") + condition + " al");
                emit("    movzx rax, al");
            }
            return;
        }

        emitExpression(node->left.get());
        if (node->left->resolved_type == "float") emit("    movq rax, xmm0");
        pushRax();
        emitExpression(node->right.get());
        if (node->right->resolved_type == "float") emit("    movq r10, xmm0");
        else emit("    mov r10, rax");
        popRcx();

        const bool floating = node->left->resolved_type == "float" ||
                              node->right->resolved_type == "float" ||
                              node->resolved_type == "float";
        if (floating) {
            if (node->left->resolved_type == "float") emit("    movq xmm0, rcx");
            else emit("    cvtsi2sd xmm0, rcx");
            if (node->right->resolved_type == "float") emit("    movq xmm1, r10");
            else emit("    cvtsi2sd xmm1, r10");
            emitFloatOperation(op);
        } else {
            emitIntOperation(op);
        }
    }

    void emitModulo97() {
        const auto adjust = label("mod97_adjust");
        emit("    mov r11, rax");
        emit("    sar r11, 63");
        emit("    xor rax, r11");
        emit("    sub rax, r11");
        emit("    mov r9, rax");
        emit("    movabs r10, 190172619316593316");
        emit("    mul r10");
        emit("    imul rax, rdx, 97");
        emit("    cmp rax, r9");
        emit("    jbe " + adjust);
        emit("    sub rdx, 1");
        emit(adjust + ":");
        emit("    imul rdx, rdx, 97");
        emit("    mov rax, r9");
        emit("    sub rax, rdx");
        emit("    xor rax, r11");
        emit("    sub rax, r11");
    }

    void emitIntOperation(const std::string& op) {
        if (op == "+") emit("    add rax, rcx");
        else if (op == "-") {
            emit("    sub rcx, rax");
            emit("    mov rax, rcx");
        } else if (op == "*") emit("    imul rax, rcx");
        else if (op == "/" || op == "//" || op == "%") {
            emit("    mov r11, rax");
            emit("    mov rax, rcx");
            emit("    cqo");
            emit("    idiv r11");
            if (op == "%") emit("    mov rax, rdx");
        } else if (op == "==" || op == "!=" || op == "<" || op == ">" ||
                   op == "<=" || op == ">=") {
            emit("    cmp rcx, r10");
            const char* condition = op == "==" ? "sete" : op == "!=" ? "setne" :
                op == "<" ? "setl" : op == ">" ? "setg" :
                op == "<=" ? "setle" : "setge";
            emit(std::string("    ") + condition + " al");
            emit("    movzx rax, al");
        } else {
            throw CompileError("unsupported integer operator '" + op + "'");
        }
    }

    void emitFloatOperation(const std::string& op) {
        if (op == "+") emit("    addsd xmm0, xmm1");
        else if (op == "-") emit("    subsd xmm0, xmm1");
        else if (op == "*") emit("    mulsd xmm0, xmm1");
        else if (op == "/") emit("    divsd xmm0, xmm1");
        else if (op == "==" || op == "!=" || op == "<" || op == ">" ||
                 op == "<=" || op == ">=") {
            emit("    ucomisd xmm0, xmm1");
            const char* condition = op == "==" ? "sete" : op == "!=" ? "setne" :
                op == "<" ? "setb" : op == ">" ? "seta" :
                op == "<=" ? "setbe" : "setae";
            emit(std::string("    ") + condition + " al");
            if (op == "==" || op == "<" || op == "<=") {
                emit("    setnp dl");
                emit("    and al, dl");
            } else if (op == "!=") {
                emit("    setp dl");
                emit("    or al, dl");
            }
            emit("    movzx rax, al");
        } else {
            throw CompileError("unsupported floating-point operator '" + op + "'");
        }
    }

    void emitCall(const Expr* node) {
        if (!node->left || node->left->kind != Expr::Kind::Name)
            fail(node->token, "only direct function calls are supported");
        if (node->left->token.value == "array") {
            emitExpression(node->arguments[0].get());
            pushCallerSavedLocals();
            emit("    mov rdi, rax");
            emit("    mov esi, 8");
            callAligned("calloc@PLT");
            popCallerSavedLocals();
            return;
        }
        if (node->arguments.size() > 6)
            fail(node->token, "the native backend currently supports at most six arguments");

        const Stmt* target = nullptr;
        for (const auto& candidate : program_) {
            if (candidate->kind == Stmt::Kind::Function &&
                candidate->name == node->left->token.value) {
                target = candidate.get();
                break;
            }
        }
        if (!target) fail(node->token, "unknown function in native backend");

        for (std::size_t i = 0; i < node->arguments.size(); ++i) {
            const auto& argument = node->arguments[i];
            emitExpression(argument.get());
            const auto& expected = target->parameters[i].type;
            if (expected == "float" && argument->resolved_type == "int")
                emit("    cvtsi2sd xmm0, rax");
            if (expected == "float") emit("    movq rax, xmm0");
            pushRax();
        }
        pushCallerSavedLocals();
        for (std::size_t i = 0; i < node->arguments.size(); ++i) {
            const auto offset = 16 + (node->arguments.size() - 1 - i) * 8;
            emit("    mov " + std::string(argument_registers_[i]) +
                 ", QWORD PTR [rsp + " + std::to_string(offset) + "]");
        }
        const std::string function = target->name == "main"
            ? "main" : "swan_fn_" + target->name;
        callAligned(function);
        popCallerSavedLocals();
        if (!node->arguments.empty()) {
            const auto bytes = node->arguments.size() * 8;
            emit("    add rsp, " + std::to_string(bytes));
            stack_bytes_ -= static_cast<int>(bytes);
        }
        if (node->resolved_type == "float")
            emit("    movq xmm0, xmm0");
    }
};

inline constexpr const char* AssemblyGenerator::argument_registers_[];
} // namespace swan
