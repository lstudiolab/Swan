#pragma once
#include "swan/Support/Version.hpp"
#include <filesystem>
#include <iostream>
#include <string>
namespace swan::driver {
enum class Action { Run, Build };
struct CommandLine {
    std::filesystem::path input, output;
    Action action = Action::Run;
    bool hasOutput = false, emitAssembly = false, optimize = false, fastCompile = false;
    std::string target = "native";
    bool showHelp = false, showVersion = false;
};
inline bool parseCommandLine(int argc, char** argv, CommandLine& options) {
    int i = 1;
    if (i < argc && std::string(argv[i]) == "build") { options.action = Action::Build; ++i; }
    else if (i < argc && std::string(argv[i]) == "run") ++i;
    for (; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") { options.showVersion = true; return true; }
        if (arg == "--help" || arg == "-h") { options.showHelp = true; return true; }
        if (arg == "--emit-asm") { options.emitAssembly = true; continue; }
        if (arg == "--optimize" || arg == "-O" || arg == "-O3") { options.optimize = true; continue; }
        if (arg == "--fast-compile" || arg == "-O0") { options.fastCompile = true; continue; }
        if (arg == "--target") {
            if (++i >= argc) { std::cerr << "swan: missing target name\n"; return false; }
            options.target = argv[i]; continue;
        }
        if (arg == "-o" || arg == "--output") {
            if (++i >= argc) { std::cerr << "swan: missing output path\n"; return false; }
            options.output = argv[i]; options.hasOutput = true; continue;
        }
        if (options.input.empty()) { options.input = arg; continue; }
        std::cerr << "swan: unexpected argument '" << arg << "'\n"; return false;
    }
    if (options.input.empty()) options.showHelp = true;
    return true;
}
inline void printHelp() {
    std::cout << "Swan " << versionString() << "\n"
              << "Usage: swan <file.swan> [options]  Compile and run\n"
              << "       swan build <file.swan>      Build native executable\n"
              << "       swan <file.swan> --emit-asm  Emit target assembly\n"
              << "       swan <file.swan> --target TRIPLE --emit-asm\n"
              << "Targets: native, x86_64, aarch64, armv7, riscv64, wasm32, or LLVM target triple\n"
              << "Options: -O / --optimize, --fast-compile, -o PATH\n";
}
} // namespace swan::driver
