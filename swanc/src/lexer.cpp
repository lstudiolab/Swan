#include "swan/lexer.hpp"
#include <cctype>
#include <unordered_map>
namespace swan {
static const std::unordered_map<std::string,TokenKind> keywords={
 {"func",TokenKind::KwFunc},{"struct",TokenKind::KwStruct},{"false",TokenKind::KwFalse},{"true",TokenKind::KwTrue},
 {"if",TokenKind::KwIf},{"else",TokenKind::KwElse},{"while",TokenKind::KwWhile},{"as",TokenKind::KwAs},
 {"from",TokenKind::KwFrom},{"loop",TokenKind::KwLoop},{"null",TokenKind::KwNull},{"return",TokenKind::KwReturn},
 {"guard",TokenKind::KwGuard},{"in",TokenKind::KwIn},{"block",TokenKind::KwBlock},{"break",TokenKind::KwBreak},
 {"continue",TokenKind::KwContinue},{"const",TokenKind::KwConst},{"var",TokenKind::KwVar},{"let",TokenKind::KwLet},
 {"extern",TokenKind::KwExtern},{"intern",TokenKind::KwIntern},{"import",TokenKind::KwImport},{"enum",TokenKind::KwEnum},
 {"type",TokenKind::KwType},{"int",TokenKind::KwInt},{"init",TokenKind::KwInit},{"deinit",TokenKind::KwDeinit},
 {"bool",TokenKind::KwBool},{"i8",TokenKind::KwI8},{"i16",TokenKind::KwI16},{"i32",TokenKind::KwI32},{"i64",TokenKind::KwI64},
 {"u8",TokenKind::KwU8},{"u16",TokenKind::KwU16},{"u32",TokenKind::KwU32},{"u64",TokenKind::KwU64},
 {"varint",TokenKind::KwVarint},{"f32",TokenKind::KwF32},{"f64",TokenKind::KwF64}};
const char* tokenName(TokenKind k) { switch(k) {
#define T(x) case TokenKind::x:return #x
T(End);T(Identifier);T(Integer);T(Float);T(String);T(LParen);T(RParen);T(LBrace);T(RBrace);T(Comma);T(Colon);T(Semicolon);T(Plus);T(Minus);T(Star);T(Slash);T(Percent);T(Equal);T(EqualEqual);T(Bang);T(BangEqual);T(Less);T(LessEqual);T(Greater);T(GreaterEqual);T(Arrow);default:return "token";}}
char Lexer::peek(std::size_t n) const { return pos_+n<source_.size()?source_[pos_+n]:'\\0'; }
char Lexer::take(){char c=peek();if(c){++pos_;if(c=='\\n'){++line_;column_=1;}else ++column_;}return c;}
Token Lexer::make(TokenKind k,std::size_t s,SourceLocation l)const{return {k,source_.substr(s,pos_-s),l};}
void Lexer::skipTrivia(){for(;;){while(std::isspace(static_cast<unsigned char>(peek())))take();if(peek()=='/'&&peek(1)=='/'){while(peek()&&peek()!='\\n')take();continue;}if(peek()=='/'&&peek(1)=='*'){take();take();while(peek()&&!(peek()=='*'&&peek(1)=='/'))take();if(peek()){take();take();}else errors_.push_back("unterminated block comment");continue;}break;}}
std::vector<Token> Lexer::scan(){std::vector<Token> out;while(true){skipTrivia();auto start=pos_;SourceLocation loc{pos_,line_,column_};char c=take();if(!c){out.push_back({TokenKind::End,"",loc});break;}
if(std::isalpha(static_cast<unsigned char>(c))||c=='_'){while(std::isalnum(static_cast<unsigned char>(peek()))||peek()=='_')take();auto t=make(TokenKind::Identifier,start,loc);auto it=keywords.find(t.lexeme);if(it!=keywords.end())t.kind=it->second;out.push_back(std::move(t));continue;}
if(std::isdigit(static_cast<unsigned char>(c))){while(std::isdigit(static_cast<unsigned char>(peek())))take();bool f=false;if(peek()=='.'&&std::isdigit(static_cast<unsigned char>(peek(1)))){f=true;take();while(std::isdigit(static_cast<unsigned char>(peek())))take();}out.push_back(make(f?TokenKind::Float:TokenKind::Integer,start,loc));continue;}
if(c=='"'){while(peek()&&peek()!='"'&&peek()!='\\n'){if(peek()=='\\\\')take();take();}if(peek()=='"')take();else errors_.push_back("unterminated string literal");out.push_back(make(TokenKind::String,start,loc));continue;}
TokenKind k=TokenKind::Unknown;switch(c){case '(':k=TokenKind::LParen;break;case ')':k=TokenKind::RParen;break;case '{':k=TokenKind::LBrace;break;case '}':k=TokenKind::RBrace;break;case ',':k=TokenKind::Comma;break;case ':':k=TokenKind::Colon;break;case ';':k=TokenKind::Semicolon;break;case '+':k=TokenKind::Plus;break;case '-':k=peek()=='>'?(take(),TokenKind::Arrow):TokenKind::Minus;break;case '*':k=TokenKind::Star;break;case '/':k=TokenKind::Slash;break;case '%':k=TokenKind::Percent;break;case '=':k=peek()=='='?(take(),TokenKind::EqualEqual):TokenKind::Equal;break;case '!':k=peek()=='='?(take(),TokenKind::BangEqual):TokenKind::Bang;break;case '<':k=peek()=='='?(take(),TokenKind::LessEqual):TokenKind::Less;break;case '>':k=peek()=='='?(take(),TokenKind::GreaterEqual):TokenKind::Greater;break;default:errors_.push_back("unexpected character at line "+std::to_string(line_));}
out.push_back(make(k,start,loc));}return out;}
}
