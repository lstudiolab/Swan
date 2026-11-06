#include "swan/Lexer.hpp"
#include <cctype>
#include <string>
#include <unordered_set>

namespace swan {
Lexer::Lexer(std::string_view source) : source_(source) {}
bool Lexer::eof() const { return index_ >= source_.size(); }
char Lexer::peek(std::size_t offset) const {
    return index_ + offset < source_.size() ? source_[index_ + offset] : '\0';
}
char Lexer::advance() {
    const char value = source_[index_++];
    if (value == '\n') { ++line_; column_ = 1; }
    else { ++column_; }
    return value;
}
std::vector<Token> Lexer::scan() {
    static const std::unordered_set<std::string> keywords = {
        "fn","let","var","const","struct","if","else","while","return",
        "break","continue","pass","log","true","false","none","int",
        "float","bool","string","void","and","or","not","import","as",
        "for","loop","async","await","sync","hold","store","operate",
        "clone","get","init","deinit","release","enum","class","type",
        "public","private","export","self","case","range"
    };
    std::vector<Token> result;
    while (!eof()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r') { advance(); continue; }
        if (c == '#') { while (!eof() && peek() != '\n') advance(); continue; }
        const auto line = line_;
        const auto column = column_;
        if (c == '\n') {
            result.push_back({"NEWLINE","\\n",line,column});
            advance();
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            std::string value;
            while (!eof() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_'))
                value += advance();
            result.push_back({keywords.contains(value) ? value : "IDENT",value,line,column});
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            std::string value;
            while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) value += advance();
            std::string kind = "INT";
            if (!eof() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peek(1)))) {
                kind = "FLOAT"; value += advance();
                while (!eof() && std::isdigit(static_cast<unsigned char>(peek()))) value += advance();
            }
            result.push_back({kind,value,line,column});
            continue;
        }
        if (c == '"' || c == '\'') {
            const char quote = advance();
            std::string value;
            while (!eof() && peek() != quote) {
                char ch = advance();
                if (ch == '\\') {
                    if (eof()) fail({"STRING",value,line,column},"unterminated escape sequence");
                    switch (advance()) {
                        case 'n': value += '\n'; break;
                        case 'r': value += '\r'; break;
                        case 't': value += '\t'; break;
                        case '\\': value += '\\'; break;
                        case '"': value += '"'; break;
                        case '\'': value += '\''; break;
                        default: fail({"STRING",value,line,column},"unknown escape sequence");
                    }
                } else value += ch;
            }
            if (eof()) fail({"STRING",value,line,column},"unterminated string");
            advance();
            result.push_back({"STRING",value,line,column});
            continue;
        }
        const std::string pair{c,peek(1)};
        if (pair == "->" || pair == "==" || pair == "!=" || pair == "<=" ||
            pair == ">=" || pair == "&&" || pair == "||") {
            advance(); advance(); result.push_back({pair,pair,line,column}); continue;
        }
        if (std::string_view("{}()[],:;.+-*/%=<>!").find(c) != std::string_view::npos) {
            advance(); result.push_back({std::string(1,c),std::string(1,c),line,column}); continue;
        }
        fail({"","",line,column},std::string("unexpected character '") + c + "'");
    }
    result.push_back({"EOF","",line_,column_});
    return result;
}
}
