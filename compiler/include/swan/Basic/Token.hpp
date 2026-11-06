#pragma once
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
namespace swan {
struct Token {
    std::string kind;
    std::string value;
    std::size_t line = 1;
    std::size_t column = 1;
};
class CompileError : public std::runtime_error {
public:
    CompileError(const Token& token, std::string_view message)
        : std::runtime_error(std::to_string(token.line) + ":" +
              std::to_string(token.column) + ": " + std::string(message)) {}
    explicit CompileError(std::string_view message)
        : std::runtime_error(std::string(message)) {}
};
[[noreturn]] inline void fail(const Token& token, std::string_view message) {
    throw CompileError(token, message);
}
}
