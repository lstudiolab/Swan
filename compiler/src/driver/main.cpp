#include "swan/Compiler.hpp"
#include "swan/Token.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {
constexpr const char* version = "0.5.0";
void help() {
    std::cout << "Swan " << version << "\n"
              << "Usage: swan <source.swan> [-o output.c]\n";
}
}
int main(int argc,char** argv) {
    std::filesystem::path input,output;
    bool hasOutput = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--version" || argument == "-v") {
            std::cout << "Swan " << version << '\n'; return 0;
        }
        if (argument == "--help" || argument == "-h") { help(); return 0; }
        if (argument == "-o" || argument == "--output") {
            if (++i >= argc) { std::cerr << "swan: missing output path\n"; return 2; }
            output = argv[i]; hasOutput = true;
        } else if (input.empty()) input = argument;
        else { std::cerr << "swan: unexpected argument '" << argument << "'\n"; return 2; }
    }
    if (input.empty()) { help(); return 2; }
    try {
        if (input.extension() != ".swan") throw swan::CompileError("input file must use .swan extension");
        std::ifstream sourceFile(input,std::ios::binary);
        if (!sourceFile) throw swan::CompileError("cannot open '" + input.string() + "'");
        const std::string source((std::istreambuf_iterator<char>(sourceFile)),{});
        const std::string generated = swan::Compiler().compile(source);
        if (hasOutput) {
            if (output.extension() != ".c") throw swan::CompileError("output file must use .c extension");
            std::ofstream outputFile(output,std::ios::binary);
            if (!outputFile) throw swan::CompileError("cannot write '" + output.string() + "'");
            outputFile << generated;
            if (!outputFile) throw swan::CompileError("failed while writing '" + output.string() + "'");
        } else std::cout << generated;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "swan: " << error.what() << '\n';
        return 1;
    }
}
