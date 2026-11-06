#include "Swan/Frontend/Lexer.hpp"
#include "Swan/Frontend/Parser.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: swan-parse <file.swan>\n";
        return 2;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "swan-parse: cannot open '" << argv[1] << "'\n";
        return 2;
    }

    std::string source(std::istreambuf_iterator<char>(file), {});
    swan::Lexer lexer(std::move(source));
    auto tokens = lexer.tokenize();

    for (const auto& error : lexer.errors()) {
        std::cerr << error.location.line << ':' << error.location.column
                  << ": lexer error: " << error.message << '\n';
    }

    swan::Parser parser(tokens);
    auto module = parser.parse();

    for (const auto& error : parser.errors()) {
        std::cerr << error.location.line << ':' << error.location.column
                  << ": parser error: " << error.message << '\n';
    }

    if (!lexer.errors().empty() || !parser.errors().empty()) return 1;
    std::cout << "Parsed " << module.statements.size()
              << " top-level statement(s).\n";
    return 0;
}
