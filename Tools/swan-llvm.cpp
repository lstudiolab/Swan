#include "Swan/Backend/LLVM/LLVMEmitter.hpp"
#include "Swan/Frontend/Lexer.hpp"
#include "Swan/Frontend/Parser.hpp"
#include "Swan/Sema/Sema.hpp"
#include "Swan/SIL/SIL.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace {

void printDiagnostics(const std::vector<swan::SemaDiagnostic>& diagnostics,
                      const char* stage) {
    for (const auto& diagnostic : diagnostics)
        std::cerr << diagnostic.location.line << ':' << diagnostic.location.column
                  << ": " << stage << '[' << diagnostic.code << "]: "
                  << diagnostic.message << '\n';
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 && argc != 4) {
        std::cerr << "usage: swan-llvm <file.swan> [-o output.ll]\n";
        return 2;
    }
    if (argc == 4 && std::string(argv[2]) != "-o") {
        std::cerr << "swan-llvm: expected -o before the output path\n";
        return 2;
    }

    std::ifstream file(argv[1], std::ios::binary);
    if (!file) {
        std::cerr << "swan-llvm: cannot open '" << argv[1] << "'\n";
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
    printDiagnostics(sema.diagnostics(), "sema");
    if (!sema.diagnostics().empty()) return 1;

    swan::sil::Lowerer lowerer;
    auto lowered = lowerer.lower(ast);
    printDiagnostics(lowered.diagnostics, "sil");
    if (!lowered.ok()) return 1;

    swan::sil::Optimizer optimizer;
    optimizer.optimize(lowered.module);
    std::vector<swan::SemaDiagnostic> verificationErrors;
    swan::sil::Verifier verifier;
    if (!verifier.verify(lowered.module, verificationErrors)) {
        printDiagnostics(verificationErrors, "sil verifier");
        return 1;
    }

    swan::backend::llvm::LLVMEmitter emitter;
    auto emitted = emitter.emit(lowered.module);
    printDiagnostics(emitted.diagnostics, "llvm");
    if (!emitted.ok()) return 1;

    if (argc == 4) {
        std::ofstream output(argv[3], std::ios::binary);
        if (!output) {
            std::cerr << "swan-llvm: cannot write '" << argv[3] << "'\n";
            return 2;
        }
        output << emitted.output;
        if (!output) {
            std::cerr << "swan-llvm: failed while writing '" << argv[3] << "'\n";
            return 2;
        }
    } else {
        std::cout << emitted.output;
    }
    return 0;
}
