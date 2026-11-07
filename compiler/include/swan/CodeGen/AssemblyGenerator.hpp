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
                 ".extern puts\n";

        for (const auto& node : program_) {
            if (node->kind == Stmt::Kind::Function) emitFunction(node.get());
        }

        return data_ + text_ + ".section .note.GNU-stack,\"\",@progbits\n";
    }

private:
    using Scope = std::unordered_map<std::string, int>;
    static inline constexpr const char* argument_registers_[] = {
        "rdi", "rsi", "rdx", "rcx", "r8", "r9"
    };

    const Program& program_;
    std::string text_;
    std::string data_;
    std::unordered_map<std::string, std::string> strings_;
    std::unordered_map<std::string, std::string> floats_;
    std::unordered_map<const Stmt*, int> statement_slots_;
    std::vector<Scope> scopes_;
    std::vector<std::pair<std::string, std::string>> loops_;
    int next_slot_ = 0;
    int frame_size_ = 0;
    int stack_bytes_ = 0;
    unsigned next_label_ = 0;
    std::string epilogue_;

    static int align16(int value) { return (value + 15) & ~15; }

    std::string label(const char* prefix) {
        return std::string(".L") + prefix + std::to_string(next_label_++);
    }

    void emit(const std::string& value) { text_ += value; text_.push_back('\n'); }

    int allocateSlot() {
        return (++next_slot_) * 8;
    }

    int lookupSlot(std::string_view name) const {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(std::string(name));
            if (found != scope->end()) return found->second;
        }
        throw CompileError("internal error: missing stack slot for '" + std::string(name) + "'");
    }

    void collectBlock(const std::vector<StmtPtr>& statements) {
        scopes_.emplace_back();
        for (const auto& node : statements) {
            if (node->kind == Stmt::Kind::Variable) {
                const int slot = allocateSlot();
                scopes_.back().emplace(node->name, slot);
                statement_slots_.emplace(node.get(), slot);
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

    void convertIntToFloat(const Expr& expression) {
        if (expression.resolved_type == "int")
            emit("    cvtsi2sd xmm0, rax");
    }

    void emitFunction(const Stmt* function) {
        next_slot_ = 0;
        stack_bytes_ = 0;
        statement_slots_.clear();
        scopes_.clear();
        scopes_.emplace_back();

        std::vector<int> parameter_slots;
        parameter_slots.reserve(function->parameters.size());
        for (const auto& parameter : function->parameters) {
            const int slot = allocateSlot();
            scopes_.back().emplace(parameter.name, slot);
            parameter_slots.push_back(slot);
        }
        collectBlock(function->body);
        frame_size_ = align16(next_slot_ * 8);
        epilogue_ = label("return");

        const std::string name = function->name == "main"
            ? "main" : "swan_fn_" + function->name;
        emit(".globl " + name);
        emit(".type " + name + ", @function");
        emit(name + ":");
        emit("    push rbp");
        emit("    mov rbp, rsp");
        if (frame_size_) emit("    sub rsp, " + std::to_string(frame_size_));

        for (std::size_t i = 0; i < parameter_slots.size(); ++i) {
            if (i >= 6) fail(function->parameters[i].token,
                "the native backend currently supports at most six parameters");
            emit("    mov QWORD PTR [rbp - " + std::to_string(parameter_slots[i]) +
                 "], " + argument_registers_[i]);
        }

        scopes_.clear();
        scopes_.emplace_back();
        for (std::size_t i = 0; i < function->parameters.size(); ++i)
            scopes_.back().emplace(function->parameters[i].name, parameter_slots[i]);

        emitBlock(function->body);
        if (function->return_type == "float") emit("    pxor xmm0, xmm0");
        else emit("    xor eax, eax");
        emit(epilogue_ + ":");
        emit("    mov rsp, rbp");
        emit("    pop rbp");
        emit("    ret");
        emit(".size " + name + ", .-" + name);
        emit("");
        scopes_.clear();
    }

    void emitBlock(const std::vector<StmtPtr>& statements) {
        scopes_.emplace_back();
        for (const auto& node : statements) emitStatement(node.get());
        scopes_.pop_back();
    }

    void emitStatement(const Stmt* node) {
        switch (node->kind) {
        case Stmt::Kind::Variable: {
            const int slot = statement_slots_.at(node);
            scopes_.back().emplace(node->name, slot);
            if (node->value) {
                emitExpression(node->value.get());
                if (node->type == "float" && node->value->resolved_type == "int")
                    emit("    cvtsi2sd xmm0, rax");
                if (node->type == "float" ||
                    (node->type.empty() && node->value->resolved_type == "float"))
                    emit("    movsd QWORD PTR [rbp - " + std::to_string(slot) + "], xmm0");
                else
                    emit("    mov QWORD PTR [rbp - " + std::to_string(slot) + "], rax");
            } else {
                emit("    mov QWORD PTR [rbp - " + std::to_string(slot) + "], 0");
            }
            break;
        }
        case Stmt::Kind::Log: {
            const auto type = node->value->resolved_type;
            emitExpression(node->value.get());
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
            emitExpression(node->condition.get());
            emit("    test rax, rax");
            emit("    jz " + otherwise);
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
            emitExpression(node->condition.get());
            emit("    test rax, rax");
            emit("    jz " + done);
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
            const int slot = lookupSlot(value);
            if (node->resolved_type == "float")
                emit("    movsd xmm0, QWORD PTR [rbp - " + std::to_string(slot) + "]");
            else
                emit("    mov rax, QWORD PTR [rbp - " + std::to_string(slot) + "]");
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
        case Expr::Kind::Member:
            fail(node->token, "member access is not implemented by the native backend");
        }
    }

    void emitBinary(const Expr* node) {
        const auto& op = node->token.value;
        if (op == "=") {
            emitExpression(node->right.get());
            const int slot = lookupSlot(node->left->token.value);
            if (node->left->resolved_type == "float" &&
                node->right->resolved_type == "int")
                emit("    cvtsi2sd xmm0, rax");
            if (node->left->resolved_type == "float")
                emit("    movsd QWORD PTR [rbp - " + std::to_string(slot) + "], xmm0");
            else
                emit("    mov QWORD PTR [rbp - " + std::to_string(slot) + "], rax");
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

    void emitIntOperation(const std::string& op) {
        if (op == "+") emit("    add rax, rcx");
        else if (op == "-") {
            emit("    mov r11, rcx");
            emit("    sub r11, rax");
            emit("    mov rax, r11");
        } else if (op == "*") emit("    imul rax, rcx");
        else if (op == "/" || op == "%") {
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
        for (std::size_t i = 0; i < node->arguments.size(); ++i) {
            const auto offset = (node->arguments.size() - 1 - i) * 8;
            emit("    mov " + std::string(argument_registers_[i]) +
                 ", QWORD PTR [rsp" + (offset ? " + " + std::to_string(offset) : "") + "]");
        }
        const std::string function = target->name == "main"
            ? "main" : "swan_fn_" + target->name;
        callAligned(function);
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
