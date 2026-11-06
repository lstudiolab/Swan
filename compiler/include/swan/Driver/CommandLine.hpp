#pragma once
#include "swan/Support/Version.hpp"
#include <filesystem>
#include <iostream>
#include <string>
namespace swan::driver {
struct CommandLine {
    std::filesystem::path input;
    std::filesystem::path output;
    bool hasOutput = false;
    bool showHelp = false;
    bool showVersion = false;
};
inline bool parseCommandLine(int argc, char** argv, CommandLine& options) {
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--version" || argument == "-v") {
            options.showVersion = true;
            return true;
        }
        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            return true;
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
    if (options.input.empty())
        options.showHelp = true;
    return true;
}
inline void printHelp() {
    std::cout << "Swan " << versionString() << "\n"
              << "Usage: swan <source.swan> [-o output.c]\n";
}
} // namespace swan::driver
