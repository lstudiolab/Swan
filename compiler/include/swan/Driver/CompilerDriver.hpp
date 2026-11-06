#pragma once
#include "swan/Driver/Compiler.hpp"
#include "swan/Driver/CommandLine.hpp"
#include "swan/Support/FileSystem.hpp"
#include "swan/Support/Version.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <iostream>
#include <string>

namespace swan {
inline int runCompiler(int argc, char** argv) {
    driver::CommandLine options;
    if (!driver::parseCommandLine(argc, argv, options)) {
        driver::printHelp();
        return 2;
    }
    if (options.showVersion) {
        std::cout << "Swan " << versionString() << '\n';
        return 0;
    }
    if (options.showHelp) {
        driver::printHelp();
        return 0;
    }

    try {
        if (options.input.extension() != ".swan")
            throw CompileError("input file must use .swan extension");

        std::string source;
        if (!readTextFile(options.input, source))
            throw CompileError("cannot open '" + options.input.string() + "'");

        const std::string generated = Compiler().compile(source);
        if (options.hasOutput) {
            if (options.output.extension() != ".c")
                throw CompileError("output file must use .c extension");
            if (!writeTextFile(options.output, generated))
                throw CompileError("cannot write '" + options.output.string() + "'");
        } else {
            std::cout << generated;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "swan: " << error.what() << '\n';
        return 1;
    }
}
}
