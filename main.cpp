#include "swan/Driver/Compiler.hpp"
#include "swan/Driver/CommandLine.hpp"
#include "swan/Support/FileSystem.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <iostream>

int main(int argc, char** argv) {
    swan::driver::CommandLine options;
    if (!swan::driver::parseCommandLine(argc, argv, options)) {
        swan::driver::printHelp();
        return 2;
    }
    if (options.showVersion) {
        std::cout << "Swan " << swan::versionString() << '\n';
        return 0;
    }
    if (options.showHelp) {
        swan::driver::printHelp();
        return 0;
    }

    try {
        if (options.input.extension() != ".swan")
            throw swan::CompileError("input file must use .swan extension");

        std::string source;
        if (!swan::readTextFile(options.input, source))
            throw swan::CompileError("cannot open '" + options.input.string() + "'");

        const std::string generated = swan::Compiler().compile(source);
        if (options.hasOutput) {
            if (options.output.extension() != ".c")
                throw swan::CompileError("output file must use .c extension");
            if (!swan::writeTextFile(options.output, generated))
                throw swan::CompileError("cannot write '" + options.output.string() + "'");
        } else {
            std::cout << generated;
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "swan: " << error.what() << '\n';
        return 1;
    }
}
