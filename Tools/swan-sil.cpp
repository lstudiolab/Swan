#include "Swan/Frontend/Lexer.hpp"
#include "Swan/Frontend/Parser.hpp"
#include "Swan/Sema/Sema.hpp"
#include "Swan/SIL/SIL.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: swan-sil <file.swan>\n";
        return 2;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "swan-sil: cannot open '" << argv[1] << "'\n";
        return 2;
    }

    std::string source(std::istreambuf_iterator<char>(file), {});
    swan::Lexer lexer(std::move(source));
    auto tokens = lexer.tokenize();
    for (const auto& error : lexer.errors())
        std::cerr << error.location.line << ':' << error.location.column
                  << ": lexer error: " << error.message << '\n';
    if (!lexer.errors().empty()) return 1;

    swan::Parser parser(tokens);
    auto ast = parser.parse();
    for (const auto& error : parser.errors())
        std::cerr << error.location.line << ':' << error.location.column
                  << ": parser error: " << error.message << '\n';
    if (!parser.errors().empty()) return 1;

    swan::SemanticAnalyzer sema(ast);
    sema.analyze();
    for (const auto& diagnostic : sema.diagnostics())
        std::cerr << diagnostic.location.line << ':' << diagnostic.location.column
                  << ": sema[" << diagnostic.code << "]: "
                  << diagnostic.message << '\n';
    if (!sema.diagnostics().empty()) return 1;

    swan::sil::Lowerer lowerer;
    auto result = lowerer.lower(ast);
    for (const auto& diagnostic : result.diagnostics)
        std::cerr << diagnostic.location.line << ':' << diagnostic.location.column
                  << ": sil[" << diagnostic.code << "]: "
                  << diagnostic.message << '\n';
    if (!result.ok()) return 1;

    swan::sil::Optimizer optimizer;
    optimizer.optimize(result.module);
    std::vector<swan::SemaDiagnostic> verificationErrors;
    swan::sil::Verifier verifier;
    if (!verifier.verify(result.module, verificationErrors)) {
        for (const auto& diagnostic : verificationErrors)
            std::cerr << "sil verifier[" << diagnostic.code << "]: "
                      << diagnostic.message << '\n';
        return 1;
    }

    std::cout << swan::sil::print(result.module);
    return 0;
}
