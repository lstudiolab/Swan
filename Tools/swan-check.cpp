#include "Swan/Frontend/Lexer.hpp"
#include "Swan/Frontend/Parser.hpp"
#include "Swan/Sema/Sema.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace {

void printDiagnostic(const char* stage, const swan::SourceLocation& location,
                     const std::string& message) {
    std::cerr << location.line << ':' << location.column
              << ": " << stage << ": " << message << '\n';
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: swan-check <file.swan>\n";
        return 2;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "swan-check: cannot open '" << argv[1] << "'\n";
        return 2;
    }

    std::string source(std::istreambuf_iterator<char>(file), {});
    swan::Lexer lexer(std::move(source));
    auto tokens = lexer.tokenize();

    for (const auto& error : lexer.errors())
        printDiagnostic("lexer error", error.location, error.message);
    if (!lexer.errors().empty()) return 1;

    swan::Parser parser(tokens);
    auto module = parser.parse();
    for (const auto& error : parser.errors())
        printDiagnostic("parser error", error.location, error.message);
    if (!parser.errors().empty()) return 1;

    swan::SemanticAnalyzer sema(module);
    sema.analyze();
    for (const auto& diagnostic : sema.diagnostics()) {
        const char* severity =
            diagnostic.severity == swan::DiagnosticSeverity::Error ? "error" : "warning";
        std::cerr << diagnostic.location.line << ':' << diagnostic.location.column
                  << ": " << severity << '[' << diagnostic.code << "]: "
                  << diagnostic.message << '\n';
    }

    if (!sema.diagnostics().empty()) return 1;
    std::cout << "Semantic analysis passed.\n";
    return 0;
}
