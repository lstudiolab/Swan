#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace swan {
// Swan-owned x86-64 System V backend. Emits GNU assembler Intel syntax directly.
class X86_64Generator {
    const Program& program_;
    std::string text_, rodata_;
    std::unordered_map<std::string,std::string> strings_;
    std::unordered_map<const Stmt*,int> slots_;
    std::vector<std::unordered_map<std::string,int>> scopes_;
    std::vector<std::pair<std::string,std::string>> loops_;
    std::string returnLabel_;
    int nextSlot_=0, nextLabel_=0, nextString_=0, stackDepth_=0;
    static constexpr const char* argRegs_[6]={"rdi","rsi","rdx","rcx","r8","r9"};
    void emit(const std::string& s){text_+="    "+s+"\n";}
    void push(const std::string& r){emit("push "+r);stackDepth_+=8;}
    void pop(const std::string& r){emit("pop "+r);stackDepth_-=8;}
    void reserve(int n){if(n>0){emit("sub rsp, "+std::to_string(n));stackDepth_+=n;}}
    void release(int n){if(n>0){emit("add rsp, "+std::to_string(n));stackDepth_-=n;}}
    void callSymbol(const std::string& name){int pad=(16-stackDepth_%16)%16;reserve(pad);emit("call "+name);release(pad);}
    std::string label(const char* p){return std::string(".L")+p+std::to_string(nextLabel_++);}
    std::string stringLabel(const std::string& value){
        auto f=strings_.find(value); if(f!=strings_.end()) return f->second;
        auto n=".LS"+std::to_string(nextString_++); strings_[value]=n;
        rodata_+=n+":\n    .byte ";
        for(std::size_t i=0;i<value.size();++i){if(i)rodata_+=", ";rodata_+=std::to_string(static_cast<unsigned char>(value[i]));}
        if(!value.empty())rodata_+=", "; rodata_+="0\n"; return n;
    }
    int slot(const Stmt* s) const {auto f=slots_.find(s);if(f==slots_.end())throw CompileError("missing local stack slot");return f->second;}
    int lookup(const std::string& n) const {
        for(auto i=scopes_.rbegin();i!=scopes_.rend();++i){auto f=i->find(n);if(f!=i->end())return f->second;}
        throw CompileError("unknown native variable '"+n+"'");
    }
    void collect(const std::vector<StmtPtr>& body){
        for(const auto& s:body){if(s->kind==Stmt::Kind::Variable)slots_[s.get()]=++nextSlot_;collect(s->body);collect(s->alternate);}
    }
    void expr(const Expr* e){
        const auto& v=e->token.value;
        switch(e->kind){
        case Expr::Kind::Literal:
            if(e->token.kind=="STRING")emit("lea rax, [rip + "+stringLabel(v)+"]");
            else if(e->token.kind=="true")emit("mov eax, 1");
            else if(e->token.kind=="false"||e->token.kind=="none")emit("xor eax, eax");
            else if(e->resolved_type=="float")throw CompileError("floating-point literal lowering is not implemented in the x86-64 backend yet");
            else emit("mov rax, "+v);
            return;
        case Expr::Kind::Name:emit("mov rax, QWORD PTR [rbp - "+std::to_string(lookup(v)*8)+"]");return;
        case Expr::Kind::Unary:
            expr(e->left.get());
            if(v=="-")emit("neg rax");
            else if(v=="not"||v=="!"){emit("test rax, rax");emit("sete al");emit("movzx rax, al");}
            else throw CompileError("unsupported unary operator '"+v+"'");
            return;
        case Expr::Kind::Call:call(e);return;
        case Expr::Kind::Index:
            expr(e->left.get());push("rax");expr(e->right.get());emit("mov rcx, rax");pop("rax");emit("mov rax, QWORD PTR [rax + rcx*8]");return;
        case Expr::Kind::Member:throw CompileError("struct member lowering is not implemented in the x86-64 backend yet");
        case Expr::Kind::Binary:binary(e);return;
        }
        throw CompileError("unsupported expression in x86-64 backend");
    }
    void call(const Expr* e){
        const auto& name=e->left->token.value;
        if(name=="array"){
            if(e->arguments.size()!=1)throw CompileError("array expects one argument");
            expr(e->arguments[0].get());emit("mov rdi, rax");emit("mov esi, 8");emit("xor eax, eax");callSymbol("calloc@PLT");return;
        }
        const Stmt* fn=nullptr;
        for(const auto& s:program_)if(s->kind==Stmt::Kind::Function&&s->name==name){fn=s.get();break;}
        if(!fn)throw CompileError("unknown function '"+name+"'");
        const auto n=e->arguments.size();
        if(n>6)throw CompileError("x86-64 backend currently supports at most six call arguments");
        const bool pad=(n&1)!=0;if(pad)emit("sub rsp, 8");
        for(const auto& a:e->arguments){expr(a.get());push("rax");}
        for(std::size_t i=0;i<n;++i)emit("mov "+std::string(argRegs_[i])+", QWORD PTR [rsp + "+std::to_string((n-1-i)*8)+"]");
        if(n||pad)emit("add rsp, "+std::to_string(n*8+(pad?8:0)));
        emit("xor eax, eax");emit("call "+(name=="main"?std::string("main"):"swan_fn_"+name));
    }
    void binary(const Expr* e){
        const auto& op=e->token.value;
        if(op=="="){
            if(e->left->kind==Expr::Kind::Name){expr(e->right.get());emit("mov QWORD PTR [rbp - "+std::to_string(lookup(e->left->token.value)*8)+"], rax");return;}
            if(e->left->kind==Expr::Kind::Index){expr(e->left->left.get());push("rax");expr(e->left->right.get());emit("mov rcx, rax");push("rcx");expr(e->right.get());pop("rcx");pop("rdx");emit("mov QWORD PTR [rdx + rcx*8], rax");return;}
            throw CompileError("unsupported assignment target in x86-64 backend");
        }
        expr(e->left.get());push("rax");expr(e->right.get());emit("mov rcx, rax");pop("rax");
        if(op=="+")emit("add rax, rcx");
        else if(op=="-")emit("sub rax, rcx");
        else if(op=="*")emit("imul rax, rcx");
        else if(op=="/"||op=="//"||op=="%"){emit("cqo");emit("idiv rcx");if(op=="%")emit("mov rax, rdx");}
        else if(op=="and")emit("and rax, rcx");
        else if(op=="or")emit("or rax, rcx");
        else if(op=="=="||op=="!="||op=="<"||op==">"||op=="<="||op==">="){
            emit("cmp rax, rcx");
            const char* cc=op=="=="?"sete":op=="!="?"setne":op=="<"?"setl":op==">"?"setg":op=="<="?"setle":"setge";
            emit(std::string(cc)+" al");emit("movzx rax, al");
        }else throw CompileError("unsupported binary operator '"+op+"'");
    }
    void log(const Expr* e){
        expr(e);
        if(e->resolved_type=="string")emit("mov rdi, rax");
        else{auto f=stringLabel("%lld\n");emit("mov rsi, rax");emit("lea rdi, [rip + "+f+"]");}
        emit("xor eax, eax");callSymbol(e->resolved_type=="string"?"puts@PLT":"printf@PLT");
    }
    void block(const std::vector<StmtPtr>& body){
        scopes_.emplace_back();
        for(const auto& s:body){
            if(s->kind==Stmt::Kind::Variable){int off=slot(s.get());scopes_.back()[s->name]=off;if(s->value)expr(s->value.get());else emit("xor eax, eax");emit("mov QWORD PTR [rbp - "+std::to_string(off*8)+"], rax");}
            else if(s->kind==Stmt::Kind::Log)log(s->value.get());
            else if(s->kind==Stmt::Kind::Return){if(s->value)expr(s->value.get());else emit("xor eax, eax");emit("jmp "+returnLabel_);}
            else if(s->kind==Stmt::Kind::Expression)expr(s->value.get());
            else if(s->kind==Stmt::Kind::Pass||s->kind==Stmt::Kind::Import){}
            else if(s->kind==Stmt::Kind::Break||s->kind==Stmt::Kind::Continue){if(loops_.empty())throw CompileError("loop control outside loop");emit("jmp "+(s->kind==Stmt::Kind::Break?loops_.back().first:loops_.back().second));}
            else if(s->kind==Stmt::Kind::If){auto no=label("if_no"),end=label("if_end");expr(s->condition.get());emit("test rax, rax");emit("jz "+no);block(s->body);emit("jmp "+end);text_+=no+":\n";if(!s->alternate.empty())block(s->alternate);text_+=end+":\n";}
            else if(s->kind==Stmt::Kind::While){auto cond=label("while_cond"),end=label("while_end");text_+=cond+":\n";expr(s->condition.get());emit("test rax, rax");emit("jz "+end);loops_.push_back({end,cond});block(s->body);loops_.pop_back();emit("jmp "+cond);text_+=end+":\n";}
            else throw CompileError("unsupported statement in x86-64 backend");
        }
        scopes_.pop_back();
    }
    void function(const Stmt* fn){
        nextSlot_=0;stackDepth_=0;slots_.clear();scopes_.clear();loops_.clear();
        scopes_.emplace_back();
        for(const auto& p:fn->parameters)scopes_.back()[p.name]=++nextSlot_;
        collect(fn->body);
        const auto frame=((nextSlot_*8+15)/16)*16;
        const auto name=fn->name=="main"?std::string("main"):"swan_fn_"+fn->name;
        returnLabel_=label("return");
        text_+=".globl "+name+"\n.type "+name+", @function\n"+name+":\n";
        emit("push rbp");emit("mov rbp, rsp");if(frame)emit("sub rsp, "+std::to_string(frame));
        for(std::size_t i=0;i<fn->parameters.size();++i){int off=lookup(fn->parameters[i].name);if(i<6)emit("mov QWORD PTR [rbp - "+std::to_string(off*8)+"], "+argRegs_[i]);else{emit("mov rax, QWORD PTR [rbp + "+std::to_string(16+(i-6)*8)+"]");emit("mov QWORD PTR [rbp - "+std::to_string(off*8)+"], rax");}}
        block(fn->body);
        text_+=returnLabel_+":\n";emit("leave");emit("ret");text_+=".size "+name+", .-"+name+"\n";
    }
public:
    explicit X86_64Generator(const Program& p):program_(p){}
    std::string generate(){
        text_=".intel_syntax noprefix\n.text\n";bool main=false;
        for(const auto& s:program_)if(s->kind==Stmt::Kind::Function){if(s->name=="main")main=true;function(s.get());}
        if(!main)throw CompileError("program must define fn main");
        text_+=".section .note.GNU-stack,\"\",@progbits\n";
        if(!rodata_.empty())text_+=".section .rodata\n"+rodata_;
        return text_;
    }
};
} // namespace swan
