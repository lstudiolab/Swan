#pragma once
#include "swan/AST/AST.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace swan {
// Swan-owned AArch64 AAPCS64 backend. Emits GNU assembler syntax directly.
class AArch64Generator {
    const Program& program_;
    std::string text_, rodata_;
    std::unordered_map<std::string,std::string> strings_;
    std::unordered_map<const Stmt*,int> slots_;
    std::vector<std::unordered_map<std::string,int>> scopes_;
    std::vector<std::pair<std::string,std::string>> loops_;
    std::string returnLabel_;
    int nextSlot_=0,nextLabel_=0,nextString_=0,stackDepth_=0;
    static constexpr const char* argRegs_[8]={"x0","x1","x2","x3","x4","x5","x6","x7"};
    void emit(const std::string& s){text_+="    "+s+"\n";}
    void push(const std::string& r){emit("sub sp, sp, #16");emit("str "+r+", [sp]");stackDepth_+=16;}
    void pop(const std::string& r){emit("ldr "+r+", [sp]");emit("add sp, sp, #16");stackDepth_-=16;}
    void reserve(int n){if(n>0){adjustStack(n,true);stackDepth_+=n;}}
    void release(int n){if(n>0){adjustStack(n,false);stackDepth_-=n;}}
    void adjustStack(int n,bool down){
        if(n<=4095)emit(std::string(down?"sub":"add")+" sp, sp, #"+std::to_string(n));
        else{loadImmediate("x10",static_cast<std::uint64_t>(n));emit(std::string(down?"sub":"add")+" sp, sp, x10");}
    }
    void callSymbol(const std::string& n){int pad=(16-stackDepth_%16)%16;reserve(pad);emit("bl "+n);release(pad);}
    void loadImmediate(const std::string& reg,std::uint64_t value){
        bool first=true;
        for(int shift=0;shift<64;shift+=16){
            auto part=static_cast<unsigned>((value>>shift)&0xffffu);
            if(part==0 && !(first && shift==48))continue;
            if(first){emit("movz "+reg+", #"+std::to_string(part)+(shift?", lsl #"+std::to_string(shift):""));first=false;}
            else if(part)emit("movk "+reg+", #"+std::to_string(part)+", lsl #"+std::to_string(shift));
        }
        if(first)emit("movz "+reg+", #0");
    }
    std::string label(const char* p){return std::string(".L")+p+std::to_string(nextLabel_++);}
    std::string stringLabel(const std::string& value){
        auto f=strings_.find(value);if(f!=strings_.end())return f->second;
        auto n=".LS"+std::to_string(nextString_++);strings_[value]=n;rodata_+=n+":\n    .byte ";
        for(std::size_t i=0;i<value.size();++i){if(i)rodata_+=", ";rodata_+=std::to_string(static_cast<unsigned char>(value[i]));}
        if(!value.empty())rodata_+=", ";
        rodata_+="0\n";return n;
    }
    void address(const std::string& reg,const std::string& symbol){emit("adrp "+reg+", "+symbol);emit("add "+reg+", "+reg+", :lo12:"+symbol);}
    int slot(const Stmt* s)const{auto f=slots_.find(s);if(f==slots_.end())throw CompileError("missing local stack slot");return f->second;}
    int lookup(const std::string& n)const{for(auto i=scopes_.rbegin();i!=scopes_.rend();++i){auto f=i->find(n);if(f!=i->end())return f->second;}throw CompileError("unknown native variable '"+n+"'");}
    void loadSlot(int off){int bytes=off*8;if(bytes<=256)emit("ldr x0, [x29, #-"+std::to_string(bytes)+"]");else{loadImmediate("x10",bytes);emit("sub x10, x29, x10");emit("ldr x0, [x10]");}}
    void storeSlot(int off,const std::string& reg="x0"){int bytes=off*8;if(bytes<=256)emit("str "+reg+", [x29, #-"+std::to_string(bytes)+"]");else{loadImmediate("x10",bytes);emit("sub x10, x29, x10");emit("str "+reg+", [x10]");}}
    void collect(const std::vector<StmtPtr>& body){for(const auto& s:body){if(s->kind==Stmt::Kind::Variable)slots_[s.get()]=++nextSlot_;collect(s->body);collect(s->alternate);}}
    void expr(const Expr* e){
        const auto& v=e->token.value;
        switch(e->kind){
        case Expr::Kind::Literal:
            if(e->token.kind=="STRING")address("x0",stringLabel(v));
            else if(e->token.kind=="true")emit("mov x0, #1");
            else if(e->token.kind=="false"||e->token.kind=="none")emit("mov x0, #0");
            else if(e->resolved_type=="float")throw CompileError("floating-point lowering is not implemented in the AArch64 backend yet");
            else{try{loadImmediate("x0",std::stoull(v));}catch(...){throw CompileError("invalid AArch64 integer literal '"+v+"'");}}
            return;
        case Expr::Kind::Name:loadSlot(lookup(v));return;
        case Expr::Kind::Unary:
            expr(e->left.get());
            if(v=="-")emit("neg x0, x0");
            else if(v=="not"||v=="!"){emit("cmp x0, #0");emit("cset x0, eq");}
            else throw CompileError("unsupported unary operator '"+v+"'");
            return;
        case Expr::Kind::Call:call(e);return;
        case Expr::Kind::Index:
            expr(e->left.get());push("x0");expr(e->right.get());emit("mov x9, x0");pop("x10");emit("ldr x0, [x10, x9, lsl #3]");return;
        case Expr::Kind::Member:throw CompileError("struct member lowering is not implemented in the AArch64 backend yet");
        case Expr::Kind::Binary:binary(e);return;
        }
        throw CompileError("unsupported expression in AArch64 backend");
    }
    void call(const Expr* e){
        const auto& name=e->left->token.value;
        if(name=="array"){
            if(e->arguments.size()!=1)throw CompileError("array expects one argument");
            expr(e->arguments[0].get());emit("mov x1, #8");callSymbol("calloc");return;
        }
        const Stmt* fn=nullptr;for(const auto& s:program_)if(s->kind==Stmt::Kind::Function&&s->name==name){fn=s.get();break;}
        if(!fn)throw CompileError("unknown function '"+name+"'");
        const auto n=e->arguments.size();if(n>8)throw CompileError("AArch64 backend currently supports at most eight call arguments");
        const int area=static_cast<int>(((n*8+15)/16)*16);reserve(area);
        for(std::size_t i=0;i<n;++i){expr(e->arguments[i].get());emit("str x0, [sp, #"+std::to_string(i*8)+"]");}
        for(std::size_t i=0;i<n;++i)emit("ldr "+std::string(argRegs_[i])+", [sp, #"+std::to_string(i*8)+"]");
        callSymbol(name=="main"?"main":"swan_fn_"+name);release(area);
    }
    void binary(const Expr* e){
        const auto& op=e->token.value;
        if(op=="="){
            if(e->left->kind==Expr::Kind::Name){expr(e->right.get());storeSlot(lookup(e->left->token.value));return;}
            if(e->left->kind==Expr::Kind::Index){expr(e->left->left.get());push("x0");expr(e->left->right.get());emit("mov x9, x0");push("x9");expr(e->right.get());pop("x9");pop("x10");emit("str x0, [x10, x9, lsl #3]");return;}
            throw CompileError("unsupported assignment target in AArch64 backend");
        }
        expr(e->left.get());push("x0");expr(e->right.get());emit("mov x9, x0");pop("x0");
        if(op=="+")emit("add x0, x0, x9");
        else if(op=="-")emit("sub x0, x0, x9");
        else if(op=="*")emit("mul x0, x0, x9");
        else if(op=="/"||op=="//")emit("sdiv x0, x0, x9");
        else if(op=="%"){emit("sdiv x10, x0, x9");emit("msub x0, x10, x9, x0");}
        else if(op=="and")emit("and x0, x0, x9");
        else if(op=="or")emit("orr x0, x0, x9");
        else if(op=="=="||op=="!="||op=="<"||op==">"||op=="<="||op==">="){
            emit("cmp x0, x9");
            const char* cc=op=="=="?"eq":op=="!="?"ne":op=="<"?"lt":op==">"?"gt":op=="<="?"le":"ge";
            emit(std::string("cset x0, ")+cc);
        }else throw CompileError("unsupported binary operator '"+op+"'");
    }
    void log(const Expr* e){
        expr(e);
        if(e->resolved_type=="string")callSymbol("puts");
        else{emit("mov x1, x0");address("x0",stringLabel("%lld\n"));callSymbol("printf");}
    }
    void block(const std::vector<StmtPtr>& body){
        scopes_.emplace_back();
        for(const auto& s:body){
            if(s->kind==Stmt::Kind::Variable){int off=slot(s.get());scopes_.back()[s->name]=off;if(s->value)expr(s->value.get());else emit("mov x0, #0");storeSlot(off);}
            else if(s->kind==Stmt::Kind::Log)log(s->value.get());
            else if(s->kind==Stmt::Kind::Return){if(s->value)expr(s->value.get());else emit("mov x0, #0");emit("b "+returnLabel_);}
            else if(s->kind==Stmt::Kind::Expression)expr(s->value.get());
            else if(s->kind==Stmt::Kind::Pass||s->kind==Stmt::Kind::Import){}
            else if(s->kind==Stmt::Kind::Break||s->kind==Stmt::Kind::Continue){if(loops_.empty())throw CompileError("loop control outside loop");emit("b "+(s->kind==Stmt::Kind::Break?loops_.back().first:loops_.back().second));}
            else if(s->kind==Stmt::Kind::If){auto no=label("if_no"),end=label("if_end");expr(s->condition.get());emit("cbz x0, "+no);block(s->body);emit("b "+end);text_+=no+":\n";if(!s->alternate.empty())block(s->alternate);text_+=end+":\n";}
            else if(s->kind==Stmt::Kind::While){auto cond=label("while_cond"),end=label("while_end");text_+=cond+":\n";expr(s->condition.get());emit("cbz x0, "+end);loops_.push_back({end,cond});block(s->body);loops_.pop_back();emit("b "+cond);text_+=end+":\n";}
            else throw CompileError("unsupported statement in AArch64 backend");
        }
        scopes_.pop_back();
    }
    void function(const Stmt* fn){
        nextSlot_=0;stackDepth_=0;slots_.clear();scopes_.clear();loops_.clear();scopes_.emplace_back();
        for(const auto& p:fn->parameters)scopes_.back()[p.name]=++nextSlot_;
        collect(fn->body);const int frame=((nextSlot_*8+15)/16)*16;
        const auto name=fn->name=="main"?std::string("main"):"swan_fn_"+fn->name;returnLabel_=label("return");
        text_+=".global "+name+"\n.type "+name+", %function\n"+name+":\n";
        emit("stp x29, x30, [sp, #-16]!");emit("mov x29, sp");if(frame)adjustStack(frame,true);
        for(std::size_t i=0;i<fn->parameters.size();++i){int off=lookup(fn->parameters[i].name);if(i<8)storeSlot(off,argRegs_[i]);else throw CompileError("AArch64 parameter stack lowering is not implemented beyond eight parameters");}
        block(fn->body);
        text_+=returnLabel_+":\n";emit("mov sp, x29");emit("ldp x29, x30, [sp], #16");emit("ret");text_+=".size "+name+", .-"+name+"\n";
    }
public:
    explicit AArch64Generator(const Program& p):program_(p){}
    std::string generate(){
        text_=".text\n";bool main=false;
        for(const auto& s:program_)if(s->kind==Stmt::Kind::Function){if(s->name=="main")main=true;function(s.get());}
        if(!main)throw CompileError("program must define fn main");
        text_+=".section .note.GNU-stack,\"\",%progbits\n";
        if(!rodata_.empty())text_+=".section .rodata\n"+rodata_;
        return text_;
    }
};
} // namespace swan
