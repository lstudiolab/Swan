#pragma once
#include "swan/Support/Version.hpp"
#include <filesystem>
#include <iostream>
#include <string>
namespace swan::driver {
enum class Action { Run, Build };
struct CommandLine {
    std::filesystem::path input;
    std::filesystem::path output;
    Action action = Action::Run;
    bool hasOutput = false;
    bool emitAssembly = false;
    bool emitC = false;
    bool optimize = false;
    bool fastCompile = false;
    std::string target = "native";
    bool showHelp = false;
    bool showVersion = false;
};
inline bool parseCommandLine(int argc, char** argv, CommandLine& options) {
    int index = 1;
    if (index < argc && std::string(argv[index]) == "build") {
        options.action = Action::Build;
        ++index;
    } else if (index < argc && std::string(argv[index]) == "run") {
        options.action = Action::Run;
        ++index;
    }
    for (; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--version" || argument == "-v") {
            options.showVersion = true;
            return true;
        }
        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            return true;
        }
        if (argument == "--emit-asm") {
            options.emitAssembly = true;
            continue;
        }
        if (argument == "--emit-c") {
            options.emitC = true;
            continue;
        }
        if (argument == "--optimize" || argument == "-O") {
            options.optimize = true;
            continue;
        }
        if (argument == "--fast-compile") {
            options.fastCompile = true;
            continue;
        }
        if (argument == "--target") {
            if (++index >= argc) {
                std::cerr << "swan: missing target name\\n";
                return false;
            }
            options.target = argv[index];
            continue;
        }
        if (argument == "-o" || argument == "--output") {
            if (++index >= argc) {
                std::cerr << "swan: missing output path\n";
                return false;
            }
            options.output = argv[index];
            options.hasOutput = true;
            continue;
        }
        if (options.input.empty()) {
            options.input = argument;
            continue;
        }
        std::cerr << "swan: unexpected argument '" << argument << "'\n";
        return false;
    }
    if (options.input.empty()) options.showHelp = true;
    return true;
}
inline void printHelp() {
    std::cout << "Swan " << versionString() << "\n"
              << "Usage: swan <file.swan>          Compile and run\n"
              << "       swan run <file.swan>      Compile and run\n"
              << "       swan build <file.swan>    Build a native executable\n"
              << "       swan <file.swan> --emit-asm [-o output.s]\n"
              << "Options: -o, --output PATH  Output executable or assembly file\n";
}
} // namespace swan::driver