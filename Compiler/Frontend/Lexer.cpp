#include "Swan/Frontend/Lexer.hpp"

#include <cctype>
#include <unordered_map>
#include <utility>

namespace swan {
namespace {

const std::unordered_map<std::string, TokenKind> keywords = {
    {"let", TokenKind::Let}, {"var", TokenKind::Var},
    {"const", TokenKind::Const}, {"fn", TokenKind::Fn},
    {"struct", TokenKind::Struct}, {"type", TokenKind::Type},
    {"enum", TokenKind::Enum}, {"class", TokenKind::Class},
    {"int", TokenKind::Int}, {"public", TokenKind::Public},
    {"private", TokenKind::Private}, {"import", TokenKind::Import},
    {"export", TokenKind::Export}, {"as", TokenKind::As},
    {"self", TokenKind::Self},

    {"if", TokenKind::If}, {"else", TokenKind::Else},
    {"endif", TokenKind::EndIf}, {"while", TokenKind::While},
    {"for", TokenKind::For}, {"loop", TokenKind::Loop},
    {"range", TokenKind::Range}, {"case", TokenKind::Case},
    {"break", TokenKind::Break}, {"continue", TokenKind::Continue},
    {"return", TokenKind::Return}, {"pass", TokenKind::Pass},

    {"async", TokenKind::Async}, {"await", TokenKind::Await},
    {"sync", TokenKind::Sync}, {"operate", TokenKind::Operate},

    {"hold", TokenKind::Hold}, {"store", TokenKind::Store},
    {"clone", TokenKind::Clone}, {"get", TokenKind::Get},
    {"init", TokenKind::Init}, {"deinit", TokenKind::Deinit},
    {"release", TokenKind::Release},

    {"true", TokenKind::True}, {"false", TokenKind::False},
    {"none", TokenKind::None}, {"log", TokenKind::Log},
};

bool isIdentifierStart(unsigned char c) {
    return std::isalpha(c) || c == '_' || c >= 0x80;
}

bool isIdentifierContinue(unsigned char c) {
    return std::isalnum(c) || c == '_' || c >= 0x80;
}

} // namespace

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

const std::vector<LexerError>& Lexer::errors() const noexcept {
    return errors_;
}

bool Lexer::atEnd() const noexcept {
    return offset_ >= source_.size();
}

char Lexer::current(std::size_t lookahead) const noexcept {
    const auto index = offset_ + lookahead;
    return index < source_.size() ? source_[index] : '\0';
}

char Lexer::advance() noexcept {
    if (atEnd()) return '\0';
    const char c = source_[offset_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) noexcept {
    if (atEnd() || current() != expected) return false;
    advance();
    return true;
}

SourceLocation Lexer::location() const noexcept {
    return {offset_, line_, column_};
}

void Lexer::emit(TokenKind kind, std::size_t start, SourceLocation where) {
    tokens_.push_back({kind, source_.substr(start, offset_ - start), where});
}

void Lexer::error(SourceLocation where, std::string message) {
    errors_.push_back({where, std::move(message)});
}

TokenKind Lexer::keywordKind(const std::string& text) noexcept {
    const auto found = keywords.find(text);
    return found == keywords.end() ? TokenKind::Identifier : found->second;
}

void Lexer::applyIndentation(std::size_t width, SourceLocation where) {
    const auto currentWidth = indentation_.back();
    if (width > currentWidth) {
        indentation_.push_back(width);
        tokens_.push_back({TokenKind::Indent, "", where});
        return;
    }

    if (width == currentWidth) return;

    while (indentation_.size() > 1 && width < indentation_.back()) {
        indentation_.pop_back();
        tokens_.push_back({TokenKind::Dedent, "", where});
    }

    if (width != indentation_.back()) {
        error(where, "Indentation does not match an earlier indentation level.");
        indentation_.push_back(width);
        tokens_.push_back({TokenKind::Indent, "", where});
    }
}

void Lexer::emitPendingDedents(SourceLocation where) {
    while (indentation_.size() > 1) {
        indentation_.pop_back();
        tokens_.push_back({TokenKind::Dedent, "", where});
    }
}

void Lexer::scanIdentifier() {
    const auto start = offset_;
    const auto where = location();
    while (!atEnd() && isIdentifierContinue(static_cast<unsigned char>(current()))) {
        advance();
    }
    const auto text = source_.substr(start, offset_ - start);
    tokens_.push_back({keywordKind(text), text, where});
}

void Lexer::scanNumber() {
    const auto start = offset_;
    const auto where = location();

    while (std::isdigit(static_cast<unsigned char>(current()))) advance();

    bool isFloat = false;
    if (current() == '.' && std::isdigit(static_cast<unsigned char>(current(1)))) {
        isFloat = true;
        advance();
        while (std::isdigit(static_cast<unsigned char>(current()))) advance();
    }

    emit(isFloat ? TokenKind::Float : TokenKind::Integer, start, where);
}

void Lexer::scanString(const std::string& opening, const std::string& closing) {
    const auto start = offset_;
    const auto where = location();
    for (std::size_t i = 0; i < opening.size(); ++i) advance();

    bool closed = false;
    while (!atEnd()) {
        if (current() == '\\') {
            advance();
            if (!atEnd()) advance();
            continue;
        }

        bool closingMatches = true;
        for (std::size_t i = 0; i < closing.size(); ++i) {
            if (current(i) != closing[i]) {
                closingMatches = false;
                break;
            }
        }
        if (closingMatches) {
            for (std::size_t i = 0; i < closing.size(); ++i) advance();
            closed = true;
            break;
        }

        if (current() == '\n') break;
        advance();
    }

    if (!closed) error(where, "Unterminated string literal.");
    emit(TokenKind::String, start, where);
}

void Lexer::scanLine() {
    const auto lineStart = offset_;
    const auto lineLocation = location();
    std::size_t width = 0;

    while (current() == ' ' || current() == '\t') {
        width += current() == '\t' ? 4 : 1;
        advance();
    }

    if (current() == '\n' || current() == '\r' || current() == '#' || atEnd()) {
        while (!atEnd() && current() != '\n') advance();
        if (current() == '\n') {
            const auto where = location();
            advance();
            tokens_.push_back({TokenKind::Newline, "\n", where});
        }
        return;
    }

    applyIndentation(width, lineLocation);
    while (!atEnd() && current() != '\n') {
        if (current() == ' ' || current() == '\t' || current() == '\r') {
            advance();
            continue;
        }
        if (current() == '#') {
            while (!atEnd() && current() != '\n') advance();
            break;
        }
        scanToken();
    }

    if (current() == '\n') {
        const auto where = location();
        advance();
        tokens_.push_back({TokenKind::Newline, "\n", where});
    }

    (void)lineStart;
}

void Lexer::scanToken() {
    const auto start = offset_;
    const auto where = location();
    const unsigned char c = static_cast<unsigned char>(current());

    if (isIdentifierStart(c)) {
        scanIdentifier();
        return;
    }
    if (std::isdigit(c)) {
        scanNumber();
        return;
    }

    if (current() == '"' || current() == '\'') {
        const std::string quote(1, current());
        scanString(quote, quote);
        return;
    }

    // Accept typographic quotation marks used in Swan examples as well.
    if (static_cast<unsigned char>(current()) == 0xE2 &&
        static_cast<unsigned char>(current(1)) == 0x80 &&
        (static_cast<unsigned char>(current(2)) == 0x9C ||
         static_cast<unsigned char>(current(2)) == 0x9D)) {
        const bool opening = static_cast<unsigned char>(current(2)) == 0x9C;
        const std::string delimiter = opening ? "\xE2\x80\x9C" : "\xE2\x80\x9D";
        const std::string closing = opening ? "\xE2\x80\x9D" : "\xE2\x80\x9C";
        scanString(delimiter, closing);
        return;
    }

    advance();
    switch (c) {
        case ':': emit(TokenKind::Colon, start, where); break;
        case ';': emit(TokenKind::Semicolon, start, where); break;
        case ',': emit(TokenKind::Comma, start, where); break;
        case '.': emit(TokenKind::Dot, start, where); break;
        case '(': emit(TokenKind::LeftParen, start, where); break;
        case ')': emit(TokenKind::RightParen, start, where); break;
        case '[': emit(TokenKind::LeftBracket, start, where); break;
        case ']': emit(TokenKind::RightBracket, start, where); break;
        case '{': emit(TokenKind::LeftBrace, start, where); break;
        case '}': emit(TokenKind::RightBrace, start, where); break;
        case '+': emit(TokenKind::Plus, start, where); break;
        case '-':
            if (match('>')) emit(TokenKind::Arrow, start, where);
            else emit(TokenKind::Minus, start, where);
            break;
        case '*': emit(TokenKind::Star, start, where); break;
        case '/': emit(TokenKind::Slash, start, where); break;
        case '%': emit(TokenKind::Percent, start, where); break;
        case '=':
            if (match('=')) emit(TokenKind::Equal, start, where);
            else emit(TokenKind::Assign, start, where);
            break;
        case '!':
            if (match('=')) emit(TokenKind::NotEqual, start, where);
            else emit(TokenKind::Not, start, where);
            break;
        case '<':
            if (match('=')) emit(TokenKind::LessEqual, start, where);
            else emit(TokenKind::Less, start, where);
            break;
        case '>':
            if (match('=')) emit(TokenKind::GreaterEqual, start, where);
            else emit(TokenKind::Greater, start, where);
            break;
        case '&':
            if (match('&')) emit(TokenKind::And, start, where);
            else {
                error(where, "A single '&' is not a Swan operator; use '&&'.");
                emit(TokenKind::Unknown, start, where);
            }
            break;
        case '|':
            if (match('|')) emit(TokenKind::Or, start, where);
            else {
                error(where, "A single '|' is not a Swan operator; use '||'.");
                emit(TokenKind::Unknown, start, where);
            }
            break;
        default:
            error(where, "Unexpected character in source.");
            emit(TokenKind::Unknown, start, where);
            break;
    }
}

std::vector<Token> Lexer::tokenize() {
    tokens_.clear();
    errors_.clear();
    offset_ = 0;
    line_ = 1;
    column_ = 1;
    indentation_.assign(1, 0);

    while (!atEnd()) scanLine();

    emitPendingDedents(location());
    tokens_.push_back({TokenKind::EndOfFile, "", location()});
    return tokens_;
}

const char* tokenKindName(TokenKind kind) noexcept {
    switch (kind) {
#define SWAN_TOKEN_NAME(name) case TokenKind::name: return #name
        SWAN_TOKEN_NAME(EndOfFile); SWAN_TOKEN_NAME(Newline);
        SWAN_TOKEN_NAME(Indent); SWAN_TOKEN_NAME(Dedent);
        SWAN_TOKEN_NAME(Identifier); SWAN_TOKEN_NAME(Integer);
        SWAN_TOKEN_NAME(Float); SWAN_TOKEN_NAME(String);
        SWAN_TOKEN_NAME(Let); SWAN_TOKEN_NAME(Var); SWAN_TOKEN_NAME(Const);
        SWAN_TOKEN_NAME(Fn); SWAN_TOKEN_NAME(Struct); SWAN_TOKEN_NAME(Type);
        SWAN_TOKEN_NAME(Enum); SWAN_TOKEN_NAME(Class); SWAN_TOKEN_NAME(Int);
        SWAN_TOKEN_NAME(Public); SWAN_TOKEN_NAME(Private); SWAN_TOKEN_NAME(Import);
        SWAN_TOKEN_NAME(Export); SWAN_TOKEN_NAME(As); SWAN_TOKEN_NAME(Self);
        SWAN_TOKEN_NAME(If); SWAN_TOKEN_NAME(Else); SWAN_TOKEN_NAME(EndIf);
        SWAN_TOKEN_NAME(While); SWAN_TOKEN_NAME(For); SWAN_TOKEN_NAME(Loop);
        SWAN_TOKEN_NAME(Range); SWAN_TOKEN_NAME(Case); SWAN_TOKEN_NAME(Break);
        SWAN_TOKEN_NAME(Continue); SWAN_TOKEN_NAME(Return); SWAN_TOKEN_NAME(Pass);
        SWAN_TOKEN_NAME(Async); SWAN_TOKEN_NAME(Await); SWAN_TOKEN_NAME(Sync);
        SWAN_TOKEN_NAME(Operate); SWAN_TOKEN_NAME(Hold); SWAN_TOKEN_NAME(Store);
        SWAN_TOKEN_NAME(Clone); SWAN_TOKEN_NAME(Get); SWAN_TOKEN_NAME(Init);
        SWAN_TOKEN_NAME(Deinit); SWAN_TOKEN_NAME(Release); SWAN_TOKEN_NAME(True);
        SWAN_TOKEN_NAME(False); SWAN_TOKEN_NAME(None); SWAN_TOKEN_NAME(Log);
        SWAN_TOKEN_NAME(Colon); SWAN_TOKEN_NAME(Semicolon); SWAN_TOKEN_NAME(Comma);
        SWAN_TOKEN_NAME(Dot); SWAN_TOKEN_NAME(LeftParen); SWAN_TOKEN_NAME(RightParen);
        SWAN_TOKEN_NAME(LeftBracket); SWAN_TOKEN_NAME(RightBracket);
        SWAN_TOKEN_NAME(LeftBrace); SWAN_TOKEN_NAME(RightBrace);
        SWAN_TOKEN_NAME(Assign); SWAN_TOKEN_NAME(Plus); SWAN_TOKEN_NAME(Minus);
        SWAN_TOKEN_NAME(Star); SWAN_TOKEN_NAME(Slash); SWAN_TOKEN_NAME(Percent);
        SWAN_TOKEN_NAME(Equal); SWAN_TOKEN_NAME(NotEqual); SWAN_TOKEN_NAME(Less);
        SWAN_TOKEN_NAME(LessEqual); SWAN_TOKEN_NAME(Greater);
        SWAN_TOKEN_NAME(GreaterEqual); SWAN_TOKEN_NAME(And); SWAN_TOKEN_NAME(Or);
        SWAN_TOKEN_NAME(Not); SWAN_TOKEN_NAME(Arrow); SWAN_TOKEN_NAME(Unknown);
#undef SWAN_TOKEN_NAME
    }
    return "Unknown";
}

} // namespace swan
