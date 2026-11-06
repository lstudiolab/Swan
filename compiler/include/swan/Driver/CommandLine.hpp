#pragma once
#include <filesystem>
#include <iostream>
#include <string>
namespace swan {
struct CommandLineOptions {
    std::filesystem::path input, output;
    bool hasOutput = false, showHelp = false, showVersion = false;
};
inline bool parseCommandLine(int argc, char** argv, CommandLineOptions& options) {
    for (int i=1; i<argc; ++i) {
        const std::string argument=argv[i];
        if (argument=="--version" || argument=="-v") options.showVersion=true;
        else if (argument=="--help" || argument=="-h") options.showHelp=true;
        else if (argument=="-o" || argument=="--output") {
            if (++i>=argc) { std::cerr<<"swan: missing output path\n"; return false; }
            options.output=argv[i]; options.hasOutput=true;
        } else if (options.input.empty()) options.input=argument;
        else { std::cerr<<"swan: unexpected argument '"<<argument<<"'\n"; return false; }
    }
    return true;
}
}
