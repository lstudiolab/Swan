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
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--version" || argument == "-v") {
            options.showVersion = true;
            return true;
        }
        if (argument == "--help" || argument == "-h") {
            options.showHelp = true;
            return true;
        }
        if (argument == "-o" || argument == "--output") {
            if (++i >= argc) {
                std::cerr << "swan: missing output path\n";
                return false;
            }
            options.output = argv[i];
            options.hasOutput = true;
        } else if (options.input.empty()) {
            options.input = argument;
        } else {
            std::cerr << "swan: unexpected argument '" << argument << "'\n";
            return false;
        }
    }
    if (options.input.empty()) {
        options.showHelp = true;
        return false;
    }
    return true;
}
inline void printHelp() {
    std::cout << "Swan " << versionString() << "\n"
              << "Usage: swan <source.swan> [-o output.c]\n";
}
}
