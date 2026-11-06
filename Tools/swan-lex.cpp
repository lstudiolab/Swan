#include "Swan/Frontend/Lexer.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

int main(int argc, char** argv) {
    std::string source;

    if (argc > 1) {
        std::ifstream file(argv[1], std::ios::binary);
        if (!file) {
            std::cerr << "swan-lex: cannot open '" << argv[1] << "'\n";
            return 2;
        }
        source.assign(std::istreambuf_iterator<char>(file), {});
    } else {
        source.assign(std::istreambuf_iterator<char>(std::cin), {});
    }

    swan::Lexer lexer(std::move(source));
    const auto tokens = lexer.tokenize();

    for (const auto& token : tokens) {
        std::cout << token.location.line << ':'
                  << token.location.column << "  "
                  << swan::tokenKindName(token.kind);

        if (!token.lexeme.empty()) {
            std::cout << "  " << token.lexeme;
        }
        std::cout << '\n';
    }

    for (const auto& error : lexer.errors()) {
        std::cerr << error.location.line << ':'
                  << error.location.column << ": error: "
                  << error.message << '\n';
    }

    return lexer.errors().empty() ? 0 : 1;
}
