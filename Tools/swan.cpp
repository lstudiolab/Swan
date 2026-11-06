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

void printErrors(const std::vector<swan::SemaDiagnostic>& diagnostics,
                 const char* stage) {
    for (const auto& diagnostic : diagnostics)
        std::cerr << diagnostic.location.line << ':' << diagnostic.location.column
                  << ": " << stage << '[' << diagnostic.code << "]: "
                  << diagnostic.message << '\n';
}

bool writeOutput(const std::string& path, const std::string& content) {
    if (path.empty()) {
        std::cout << content;
        return static_cast<bool>(std::cout);
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "swan: cannot write '" << path << "'\n";
        return false;
    }
    file << content;
    if (!file) {
        std::cerr << "swan: failed while writing '" << path << "'\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 5) {
        std::cerr << "usage: swan <check|emit-sil|emit-llvm> <file.swan> [-o output]\n";
        return 2;
    }

    const std::string mode = argv[1];
    const std::string inputPath = argv[2];
    std::string outputPath;
    if (argc == 5) {
        if (std::string(argv[3]) != "-o") {
            std::cerr << "swan: expected -o before output path\n";
            return 2;
        }
        outputPath = argv[4];
    } else if (argc == 4) {
        std::cerr << "swan: incomplete output option\n";
        return 2;
    }

    if (mode != "check" && mode != "emit-sil" && mode != "emit-llvm") {
        std::cerr << "swan: unknown mode '" << mode << "'\n";
        return 2;
    }
    if (mode == "check" && !outputPath.empty()) {
        std::cerr << "swan: check mode does not accept an output file\n";
        return 2;
    }

    std::ifstream file(inputPath, std::ios::binary);
    if (!file) {
        std::cerr << "swan: cannot open '" << inputPath << "'\n";
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
    printErrors(sema.diagnostics(), "sema");
    if (!sema.diagnostics().empty()) return 1;

    if (mode == "check") {
        std::cout << "Swan semantic analysis passed.\n";
        return 0;
    }

    swan::sil::Lowerer lowerer;
    auto lowered = lowerer.lower(ast);
    printErrors(lowered.diagnostics, "sil");
    if (!lowered.ok()) return 1;

    swan::sil::Optimizer optimizer;
    optimizer.optimize(lowered.module);
    std::vector<swan::SemaDiagnostic> verificationErrors;
    swan::sil::Verifier verifier;
    if (!verifier.verify(lowered.module, verificationErrors)) {
        printErrors(verificationErrors, "sil verifier");
        return 1;
    }

    if (mode == "emit-sil")
        return writeOutput(outputPath, swan::sil::print(lowered.module)) ? 0 : 2;

    swan::backend::llvm::LLVMEmitter emitter;
    auto emitted = emitter.emit(lowered.module);
    printErrors(emitted.diagnostics, "llvm");
    if (!emitted.ok()) return 1;
    return writeOutput(outputPath, emitted.output) ? 0 : 2;
}
