// Swan compiler — native C++20 implementation.
// Parses and validates Swan source, then emits portable C11 source.

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace swan {
constexpr std::string_view version = "0.4.0";

struct Error : std::runtime_error { using std::runtime_error::runtime_error; };
struct Token { std::string kind, value; std::size_t line=1, column=1; };

[[noreturn]] void fail(const Token& t, std::string_view message) {
    throw Error(std::to_string(t.line)+":"+std::to_string(t.column)+": "+std::string(message));
}

class Lexer {
public:
    explicit Lexer(std::string_view source): source_(source) {}
    std::vector<Token> scan() {
        std::vector<Token> out;
        while (!eof()) {
            char c=peek();
            if (c==' '||c=='\t'||c=='\r') { advance(); continue; }
            if (c=='\n') { out.push_back({ "NEWLINE","\n",line_,column_ }); advance(); continue; }
            if (c=='#') { while(!eof()&&peek()!='\n') advance(); continue; }
            auto line=line_, col=column_;
            if (std::isalpha(static_cast<unsigned char>(c))||c=='_') {
                std::string value;
                while(!eof()&&(std::isalnum(static_cast<unsigned char>(peek()))||peek()=='_')) value+=advance();
                static const std::unordered_set<std::string> words={
                    "fn","let","var","const","struct","if","else","while","return","break","continue","pass","log",
                    "true","false","none","int","float","bool","string","void","and","or","not","import","as",
                    "for","loop","async","await","sync","hold","store","operate","clone","get","init","deinit",
                    "release","enum","class","type","public","private","export","self","case","range"
                };
                out.push_back({words.contains(value)?value:"IDENT",value,line,col}); continue;
            }
            if (std::isdigit(static_cast<unsigned char>(c))) {
                std::string value;
                while(!eof()&&std::isdigit(static_cast<unsigned char>(peek()))) value+=advance();
                std::string kind="INT";
                if(!eof()&&peek()=='.'&&std::isdigit(static_cast<unsigned char>(peek(1)))) {
                    kind="FLOAT"; value+=advance();
                    while(!eof()&&std::isdigit(static_cast<unsigned char>(peek()))) value+=advance();
                }
                out.push_back({kind,value,line,col}); continue;
            }
            if(c=='"'||c=='\'') {
                char quote=advance(); std::string value;
                while(!eof()&&peek()!=quote) {
                    char x=advance();
                    if(x=='\\') {
                        if(eof()) fail({"STRING",value,line,col},"unterminated escape sequence");
                        switch(advance()) {
                            case 'n': value+='\n'; break; case 'r': value+='\r'; break;
                            case 't': value+='\t'; break; case '\\': value+='\\'; break;
                            case '"': value+='"'; break; case '\'': value+='\''; break;
                            default: fail({"STRING",value,line,col},"unknown escape sequence");
                        }
                    } else value+=x;
                }
                if(eof()) fail({"STRING",value,line,col},"unterminated string");
                advance(); out.push_back({"STRING",value,line,col}); continue;
            }
            std::string pair{c,peek(1)};
            if(pair=="->"||pair=="=="||pair=="!="||pair=="<="||pair==">="||pair=="&&"||pair=="||") {
                advance(); advance(); out.push_back({pair,pair,line,col}); continue;
            }
            if(std::string_view("{}()[],:;.+-*/%=<>!").find(c)!=std::string_view::npos) {
                advance(); out.push_back({std::string(1,c),std::string(1,c),line,col}); continue;
            }
            fail({"","",line,col},std::string("unexpected character '")+c+"'");
        }
        out.push_back({"EOF","",line_,column_}); return out;
    }
private:
    bool eof() const { return index_>=source_.size(); }
    char peek(std::size_t n=0) const { return index_+n<source_.size()?source_[index_+n]:'\0'; }
    char advance() { char c=source_[index_++]; if(c=='\n'){++line_;column_=1;}else ++column_;return c; }
    std::string_view source_; std::size_t index_=0,line_=1,column_=1;
};

struct Expr;
using E=std::shared_ptr<Expr>;
struct Expr {
    enum class Kind { Literal,Name,Unary,Binary,Call,Member } kind;
    Token token; std::string value,op,literal_kind; E left,right; std::vector<E> args;
};
struct Stmt;
using S=std::shared_ptr<Stmt>;
struct Param { std::string name,type; Token token; };
struct Field { std::string name,type; Token token; };
struct Stmt {
    enum class Kind { Function,Struct,Variable,If,While,Return,Break,Continue,Pass,Log,Expression,Import } kind;
    Token token; std::string name,type,module,alias,return_type="void"; bool mutable_value=false;
    std::vector<Param> params; std::vector<Field> fields; std::vector<S> body,alternate; E value,condition;
};

class Parser {
public:
    explicit Parser(std::vector<Token> tokens): tokens_(std::move(tokens)) {}
    std::vector<S> parse() {
        std::vector<S> nodes; separators();
        while(!check("EOF")) { auto before=pos_; nodes.push_back(declaration()); separators(); if(before==pos_) fail(peek(),"parser made no progress"); }
        return nodes;
    }
private:
    const Token& peek(std::size_t n=0) const { return tokens_[std::min(pos_+n,tokens_.size()-1)]; }
    bool check(std::string_view k) const { return peek().kind==k; }
    bool match(std::string_view k) { if(!check(k))return false; advance();return true; }
    Token advance(){auto t=peek();if(!check("EOF"))++pos_;return t;}
    Token expect(std::string_view k,std::string_view message={}){if(check(k))return advance();fail(peek(),message.empty()?"expected "+std::string(k):message);}
    void separators(){while(match("NEWLINE")||match(";")){}}
    std::string type_name(){auto t=advance();if(t.kind!="IDENT"&&t.kind!="int"&&t.kind!="float"&&t.kind!="bool"&&t.kind!="string"&&t.kind!="void")fail(t,"expected type name");return t.value;}
    std::vector<S> block(){
        expect("{","expected '{' to begin block");std::vector<S> body;separators();
        while(!check("}")&&!check("EOF")){auto before=pos_;body.push_back(declaration());separators();if(before==pos_)fail(peek(),"parser made no progress");}
        expect("}","expected '}' to close block");return body;
    }
    S declaration(){
        if(check("fn")){auto n=std::make_shared<Stmt>();n->kind=Stmt::Kind::Function;n->token=advance();n->name=expect("IDENT","expected function name").value;expect("(");
            if(!check(")"))do{auto p=expect("IDENT","expected parameter name");std::string t;if(match(":"))t=type_name();n->params.push_back({p.value,t,p});}while(match(","));
            expect(")");if(match("->"))n->return_type=type_name();n->body=block();return n;}
        if(check("struct")){auto n=std::make_shared<Stmt>();n->kind=Stmt::Kind::Struct;n->token=advance();n->name=expect("IDENT","expected structure name").value;expect("{");separators();
            while(!check("}")&&!check("EOF")){if(check("var")||check("let"))advance();auto f=expect("IDENT","expected field name");expect(":");n->fields.push_back({f.value,type_name(),f});separators();}
            expect("}");return n;}
        if(check("let")||check("var")||check("const")){auto n=std::make_shared<Stmt>();n->kind=Stmt::Kind::Variable;n->token=advance();n->mutable_value=n->token.kind=="var";n->name=expect("IDENT","expected variable name").value;if(match(":"))n->type=type_name();if(match("="))n->value=expression();if(!n->mutable_value&&!n->value)fail(n->token,"immutable declaration requires a value");return n;}
        if(check("import")){auto n=std::make_shared<Stmt>();n->kind=Stmt::Kind::Import;n->token=advance();auto m=advance();if(m.kind!="IDENT"&&m.kind!="STRING")fail(m,"expected module name");n->module=m.value;if(match("as"))n->alias=expect("IDENT").value;return n;}
        return statement();
    }
    S statement(){
        if(check("if")){auto n=std::make_shared<Stmt>();n->kind=Stmt::Kind::If;n->token=advance();n->condition=expression();n->body=block();if(match("else")){if(check("if"))n->alternate.push_back(statement());else n->alternate=block();}return n;}
        if(check("while")){auto n=std::make_shared<Stmt>();n->kind=Stmt::Kind::While;n->token=advance();n->condition=expression();n->body=block();return n;}
        if(check("return")){auto n=std::make_shared<Stmt>();n->kind=Stmt::Kind::Return;n->token=advance();if(!check("}")&&!check("NEWLINE")&&!check(";")&&!check("EOF"))n->value=expression();return n;}
        for(auto [word,kind]:std::vector<std::pair<std::string,Stmt::Kind>>{{"break",Stmt::Kind::Break},{"continue",Stmt::Kind::Continue},{"pass",Stmt::Kind::Pass},{"log",Stmt::Kind::Log}})
            if(check(word)){auto n=std::make_shared<Stmt>();n->kind=kind;n->token=advance();if(kind==Stmt::Kind::Log)n->value=expression();return n;}
        auto n=std::make_shared<Stmt>();n->kind=Stmt::Kind::Expression;n->value=expression();n->token=n->value->token;return n;
    }
    static int precedence(std::string_view op){
        if(op=="=")return 1;if(op=="or"||op=="||")return 2;if(op=="and"||op=="&&")return 3;
        if(op=="=="||op=="!=")return 4;if(op=="<"||op=="<="||op==">"||op==">=")return 5;
        if(op=="+"||op=="-")return 6;if(op=="*"||op=="/"||op=="%")return 7;return 0;
    }
    E expression(int minimum=1){
        auto left=prefix();
        while(precedence(peek().kind)>=minimum){auto op=advance();int p=precedence(op.kind);auto right=expression(op.kind=="="?p:p+1);auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Binary;n->token=op;n->op=op.value;n->left=left;n->right=right;left=n;}
        return left;
    }
    E prefix(){
        if(check("!")||check("-")||check("not")){auto op=advance();auto n=std::make_shared<Expr>();n->kind=Expr::Kind::Unary;n->token=op;n->op=op.value;n->left=expression(8);return n;}
        auto t=advance();auto n=std::make_shared<Expr>();n->token=t;
        if(t.kind=="INT"||t.kind=="FLOAT"||t.kind=="STRING"||t.kind=="true"||t.kind=="false"||t.kind=="none"){n->kind=Expr::Kind::Literal;n->value=t.value;n->literal_kind=t.kind;}
        else if(t.kind=="IDENT"){n->kind=Expr::Kind::Name;n->value=t.value;}
        else if(t.kind=="("){n=expression();expect(")","expected ')' after expression");}
        else fail(t,"expected expression");
        while(true){
            if(match("(")){auto call=std::make_shared<Expr>();call->kind=Expr::Kind::Call;call->token=t;call->left=n;if(!check(")"))do{call->args.push_back(expression());}while(match(","));expect(")","expected ')' after arguments");n=call;}
            else if(match(".")){auto m=expect("IDENT","expected member name");auto x=std::make_shared<Expr>();x->kind=Expr::Kind::Member;x->token=m;x->left=n;x->value=m.value;n=x;}
            else break;
        }
        return n;
    }
    std::vector<Token> tokens_;std::size_t pos_=0;
};

class Analyzer {
public:
    explicit Analyzer(const std::vector<S>& nodes):nodes_(nodes){}
    void run(){
        for(const auto& n:nodes_)if(n->kind==Stmt::Kind::Struct&&!types_.insert(n->name).second)fail(n->token,"type '"+n->name+"' is already defined");
        for(const auto& n:nodes_)if(n->kind==Stmt::Kind::Function&&!functions_.emplace(n->name,n).second)fail(n->token,"function '"+n->name+"' is already declared");
        for(const auto& n:nodes_){
            if(n->kind==Stmt::Kind::Struct){for(const auto& f:n->fields)if(!types_.contains(f.type))fail(f.token,"unknown field type '"+f.type+"'");}
            else if(n->kind==Stmt::Kind::Function)function(n);
            else fail(n->token,"unsupported top-level declaration");
        }
    }
private:
    using Scope=std::unordered_map<std::string,std::pair<std::string,bool>>;
    static bool numeric(std::string_view t){return t=="int"||t=="float";}
    static bool compatible(std::string_view a,std::string_view b){return a==b||(a=="float"&&b=="int");}
    void function(const S& n){
        scopes_.clear();scopes_.emplace_back();return_type_=n->return_type;
        if(!types_.contains(return_type_))fail(n->token,"unknown return type '"+return_type_+"'");
        for(const auto& p:n->params){if(p.type.empty())fail(p.token,"parameter '"+p.name+"' requires a type");if(!types_.contains(p.type))fail(p.token,"unknown parameter type '"+p.type+"'");if(!scopes_.back().emplace(p.name,std::pair{p.type,false}).second)fail(p.token,"parameter already declared");}
        for(const auto& s:n->body)statement(s);
    }
    std::optional<std::pair<std::string,bool>> lookup(const std::string& name)const{
        for(auto i=scopes_.rbegin();i!=scopes_.rend();++i){auto f=i->find(name);if(f!=i->end())return f->second;}return {};
    }
    void scoped(const std::vector<S>& body){scopes_.emplace_back();for(const auto& s:body)statement(s);scopes_.pop_back();}
    void statement(const S& n){
        switch(n->kind){
        case Stmt::Kind::Variable:{
            if(scopes_.back().contains(n->name))fail(n->token,"'"+n->name+"' is already declared in this scope");
            auto actual=n->value?std::optional<std::string>(expression(n->value)):std::nullopt;
            std::string type=n->type.empty()?actual.value_or(""):n->type;
            if(type.empty())fail(n->token,"cannot infer type for '"+n->name+"'");
            if(!types_.contains(type))fail(n->token,"unknown type '"+type+"'");
            if(actual&&!compatible(type,*actual))fail(n->token,"cannot assign "+*actual+" to "+type+" variable '"+n->name+"'");
            scopes_.back()[n->name]={type,n->mutable_value};break;}
        case Stmt::Kind::Log:{auto t=expression(n->value);if(t!="string"&&!numeric(t)&&t!="bool")fail(n->token,"log expects a string, number, or boolean");break;}
        case Stmt::Kind::Return:{auto t=n->value?expression(n->value):"void";if(!compatible(return_type_,t))fail(n->token,"function returns "+return_type_+", but this return has type "+t);break;}
        case Stmt::Kind::Expression:expression(n->value);break;
        case Stmt::Kind::If:if(expression(n->condition)!="bool")fail(n->token,"if condition must be bool");scoped(n->body);scoped(n->alternate);break;
        case Stmt::Kind::While:if(expression(n->condition)!="bool")fail(n->token,"while condition must be bool");++loop_depth_;scoped(n->body);--loop_depth_;break;
        case Stmt::Kind::Break:case Stmt::Kind::Continue:if(!loop_depth_)fail(n->token,"loop control used outside a loop");break;
        case Stmt::Kind::Pass:break;
        default:fail(n->token,"unsupported statement");
        }
    }
    std::string expression(const E& n){
        switch(n->kind){
        case Expr::Kind::Literal:if(n->literal_kind=="INT")return "int";if(n->literal_kind=="FLOAT")return "float";if(n->literal_kind=="STRING")return "string";if(n->literal_kind=="true"||n->literal_kind=="false")return "bool";return "none";
        case Expr::Kind::Name:{auto f=lookup(n->value);if(!f)fail(n->token,"unknown name '"+n->value+"'");return f->first;}
        case Expr::Kind::Unary:{auto t=expression(n->left);if(n->op=="!"||n->op=="not"){if(t!="bool")fail(n->token,"logical not requires a bool");return "bool";}if(!numeric(t))fail(n->token,"numeric negation requires a number");return t;}
        case Expr::Kind::Binary:{
            auto l=expression(n->left);
            if(n->op=="="){if(n->left->kind!=Expr::Kind::Name)fail(n->token,"assignment target must be a variable");auto f=lookup(n->left->value);if(!f)fail(n->left->token,"unknown variable '"+n->left->value+"'");if(!f->second)fail(n->left->token,"cannot assign to immutable value '"+n->left->value+"'");auto r=expression(n->right);if(!compatible(f->first,r))fail(n->token,"cannot assign "+r+" to "+f->first);return f->first;}
            auto r=expression(n->right);
            if(n->op=="and"||n->op=="or"||n->op=="&&"||n->op=="||"){if(l!="bool"||r!="bool")fail(n->token,"logical operators require bool operands");return "bool";}
            if(n->op=="=="||n->op=="!="||n->op=="<"||n->op==">"||n->op=="<="||n->op==">="){if(l!=r&&!(numeric(l)&&numeric(r)))fail(n->token,"comparison operands have incompatible types");return "bool";}
            if(!numeric(l)||!numeric(r))fail(n->token,"arithmetic operators require numeric operands");
            return l=="float"||r=="float"||n->op=="/"?"float":"int";}
        case Expr::Kind::Call:{
            if(n->left->kind!=Expr::Kind::Name)fail(n->token,"only direct function calls are supported");
            auto f=functions_.find(n->left->value);if(f==functions_.end())fail(n->token,"unknown function '"+n->left->value+"'");
            const auto& ps=f->second->params;if(ps.size()!=n->args.size())fail(n->token,"function '"+n->left->value+"' expects "+std::to_string(ps.size())+" arguments, got "+std::to_string(n->args.size()));
            for(std::size_t i=0;i<ps.size();++i){auto t=expression(n->args[i]);if(!compatible(ps[i].type,t))fail(n->args[i]->token,"argument has type "+t+"; expected "+ps[i].type);}
            return f->second->return_type;}
        case Expr::Kind::Member:fail(n->token,"member access is not supported yet");
        }
        fail(n->token,"unsupported expression");
    }
    const std::vector<S>& nodes_;std::unordered_set<std::string> types_{"int","float","bool","string","void"};
    std::unordered_map<std::string,S> functions_;std::vector<Scope> scopes_;std::string return_type_;unsigned loop_depth_=0;
};

class Emitter {
public:
    explicit Emitter(const std::vector<S>& nodes):nodes_(nodes){}
    std::string emit(){
        out_<<"#include <stdbool.h>\n#include <stdio.h>\n#include <stddef.h>\n\n";
        for(const auto& n:nodes_)if(n->kind==Stmt::Kind::Struct)structs_.insert(n->name);
        for(const auto& n:nodes_)if(n->kind==Stmt::Kind::Function)functions_[n->name]=n;
        for(const auto& n:nodes_)if(n->kind==Stmt::Kind::Struct){line("typedef struct "+n->name+" {");++indent_;for(const auto& f:n->fields)line(ctype(f.type)+" "+f.name+";");--indent_;line("} "+n->name+";");line();}
        for(const auto& n:nodes_)if(n->kind==Stmt::Kind::Function){std::string a;for(const auto& p:n->params){if(!a.empty())a+=", ";a+=ctype(p.type)+" "+p.name;}line(ctype(n->return_type)+" "+n->name+"("+(a.empty()?"void":a)+");");}
        line();for(const auto& n:nodes_)if(n->kind==Stmt::Kind::Function)function(n);return out_.str();
    }
private:
    std::string ctype(const std::string& t)const{if(t=="int")return "int";if(t=="float")return "double";if(t=="bool")return "bool";if(t=="string")return "const char *";if(t=="void")return "void";if(structs_.contains(t))return t;throw Error("unsupported type '"+t+"'");}
    void line(const std::string& s=""){if(!s.empty())out_<<std::string(indent_*4,' ')<<s;out_<<'\n';}
    void function(const S& n){
        symbols_.clear();symbol_scopes_.clear();symbol_scopes_.emplace_back();std::string a;
        for(const auto& p:n->params){if(!a.empty())a+=", ";a+=ctype(p.type)+" "+p.name;symbols_[p.name]=p.type;symbol_scopes_.back()[p.name]=std::nullopt;}
        line(ctype(n->return_type)+" "+n->name+"("+(a.empty()?"void":a)+") {");++indent_;for(const auto& s:n->body)statement(s);--indent_;line("}");line();
    }
    std::string infer(const E& n)const{
        if(!n)return {};
        switch(n->kind){
        case Expr::Kind::Literal:if(n->literal_kind=="INT")return "int";if(n->literal_kind=="FLOAT")return "float";if(n->literal_kind=="STRING")return "string";if(n->literal_kind=="true"||n->literal_kind=="false")return "bool";return {};
        case Expr::Kind::Name:{auto f=symbols_.find(n->value);return f==symbols_.end()?"":f->second;}
        case Expr::Kind::Call:if(n->left->kind==Expr::Kind::Name){auto f=functions_.find(n->left->value);if(f!=functions_.end())return f->second->return_type;}return {};
        case Expr::Kind::Binary:if(n->op=="=="||n->op=="!="||n->op=="<"||n->op==">"||n->op=="<="||n->op==">="||n->op=="and"||n->op=="or"||n->op=="&&"||n->op=="||")return "bool";if(n->op=="/")return "float";{auto l=infer(n->left),r=infer(n->right);return l=="float"||r=="float"?"float":l;}
        default:return {};
        }
    }
    std::string quote(const std::string& v)const{
        std::ostringstream s;s<<'"';for(unsigned char c:v){switch(c){case '\\':s<<"\\\\";break;case '"':s<<"\\\"";break;case '\n':s<<"\\n";break;case '\r':s<<"\\r";break;case '\t':s<<"\\t";break;default:if(c<32||c>=127){const char*h="0123456789abcdef";s<<"\\x"<<h[c>>4]<<h[c&15]<<"\"\"";}else s<<static_cast<char>(c);}}s<<'"';return s.str();
    }
    std::string expr(const E& n)const{
        switch(n->kind){
        case Expr::Kind::Literal:if(n->literal_kind=="STRING")return quote(n->value);if(n->literal_kind=="none")return "NULL";return n->value;
        case Expr::Kind::Name:return n->value;
        case Expr::Kind::Unary:return (n->op=="not"?"!":n->op)+expr(n->left);
        case Expr::Kind::Binary:{auto l=expr(n->left),r=expr(n->right);if(n->op=="/")return "((double)("+l+") / (double)("+r+"))";auto op=n->op=="and"?"&&":n->op=="or"?"||":n->op;return "("+l+" "+op+" "+r+")";}
        case Expr::Kind::Call:{std::string s=expr(n->left)+"(";for(std::size_t i=0;i<n->args.size();++i){if(i)s+=", ";s+=expr(n->args[i]);}return s+")";}
        case Expr::Kind::Member:return expr(n->left)+"."+n->value;
        }
        return {};
    }
    void scoped_emit(const std::vector<S>& body){
        symbol_scopes_.emplace_back();for(const auto& s:body)statement(s);
        for(const auto& [name,old]:symbol_scopes_.back()){if(old)symbols_[name]=*old;else symbols_.erase(name);}symbol_scopes_.pop_back();
    }
    void statement(const S& n){
        switch(n->kind){
        case Stmt::Kind::Variable:{
            auto t=n->type.empty()?infer(n->value):n->type;if(t.empty())fail(n->token,"cannot infer type for '"+n->name+"'");
            if(symbol_scopes_.empty())symbol_scopes_.emplace_back();
            if(!symbol_scopes_.back().contains(n->name)){auto old=symbols_.find(n->name);symbol_scopes_.back()[n->name]=old==symbols_.end()?std::nullopt:std::optional<std::string>(old->second);}
            symbols_[n->name]=t;line(std::string(n->mutable_value?"":"const ")+ctype(t)+" "+n->name+(n->value?" = "+expr(n->value):"")+";");break;}
        case Stmt::Kind::Log:{auto t=infer(n->value);if(t=="string")line("puts("+expr(n->value)+");");else if(t=="float")line("printf(\"%g\\n\", (double)("+expr(n->value)+"));");else if(t=="int"||t=="bool")line("printf(\"%d\\n\", (int)("+expr(n->value)+"));");else fail(n->token,"log requires a string, number, or boolean");break;}
        case Stmt::Kind::Return:line(n->value?"return "+expr(n->value)+";":"return;");break;
        case Stmt::Kind::Expression:line(expr(n->value)+";");break;
        case Stmt::Kind::If:line("if ("+expr(n->condition)+") {");++indent_;scoped_emit(n->body);--indent_;if(!n->alternate.empty()){line("} else {");++indent_;scoped_emit(n->alternate);--indent_;}line("}");break;
        case Stmt::Kind::While:line("while ("+expr(n->condition)+") {");++indent_;scoped_emit(n->body);--indent_;line("}");break;
        case Stmt::Kind::Break:line("break;");break;case Stmt::Kind::Continue:line("continue;");break;case Stmt::Kind::Pass:line(";");break;
        default:fail(n->token,"unsupported statement in C emitter");
        }
    }
    const std::vector<S>& nodes_;std::ostringstream out_;unsigned indent_=0;
    std::unordered_map<std::string,std::string> symbols_;std::vector<std::unordered_map<std::string,std::optional<std::string>>> symbol_scopes_;
    std::unordered_map<std::string,S> functions_;std::unordered_set<std::string> structs_;
};

std::string transpile(std::string_view source){auto tokens=Lexer(source).scan();auto nodes=Parser(std::move(tokens)).parse();Analyzer(nodes).run();return Emitter(nodes).emit();}

int run(int argc,char** argv){
    std::filesystem::path input,output;bool has_output=false;
    for(int i=1;i<argc;++i){std::string a=argv[i];if(a=="--version"||a=="-v"){std::cout<<"Swan "<<version<<'\n';return 0;}if(a=="--help"||a=="-h"){std::cout<<"Swan "<<version<<"\nUsage: swan <source.swan> [-o output.c]\n";return 0;}if(a=="-o"||a=="--output"){if(++i>=argc){std::cerr<<"swan: missing output path\n";return 2;}output=argv[i];has_output=true;}else if(input.empty())input=a;else{std::cerr<<"swan: unexpected argument '"<<a<<"'\n";return 2;}}
    if(input.empty()){std::cerr<<"swan: expected an input .swan file (try --help)\n";return 2;}
    try{
        if(input.extension()!=".swan")throw Error("input file must use the .swan extension");
        std::ifstream file(input,std::ios::binary);if(!file)throw Error("cannot open '"+input.string()+"'");
        std::string source((std::istreambuf_iterator<char>(file)),{});auto result=transpile(source);
        if(has_output){if(output.extension()!=".c")throw Error("output file must use the .c extension");std::ofstream out(output,std::ios::binary);if(!out)throw Error("cannot write '"+output.string()+"'");out<<result;if(!out)throw Error("failed while writing '"+output.string()+"'");}
        else std::cout<<result;
        return 0;
    }catch(const std::exception& e){std::cerr<<"swan: "<<e.what()<<'\n';return 1;}
}
} // namespace swan

int main(int argc,char** argv){return swan::run(argc,argv);}
