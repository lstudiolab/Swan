#pragma once
#include "swan/Driver/Compiler.hpp"
#include "swan/Driver/CommandLine.hpp"
#include "swan/Support/FileSystem.hpp"
#include <iostream>
namespace swan {
inline constexpr const char* CompilerVersion="0.5.0";
inline void printHelp() {
    std::cout<<"Swan "<<CompilerVersion<<"\nUsage: swan <source.swan> [-o output.c]\n";
}
inline int runCompiler(int argc,char** argv) {
    CommandLineOptions options;
    if (!parseCommandLine(argc,argv,options)) return 2;
    if (options.showVersion) { std::cout<<"Swan "<<CompilerVersion<<'\n'; return 0; }
    if (options.showHelp) { printHelp(); return 0; }
    if (options.input.empty()) { printHelp(); return 2; }
    try {
        if (options.input.extension()!=".swan") throw CompileError("input file must use .swan extension");
        std::string source;
        if (!readTextFile(options.input,source)) throw CompileError("cannot open '"+options.input.string()+"'");
        const auto generated=Compiler().compile(source);
        if (options.hasOutput) {
            if (options.output.extension()!=".c") throw CompileError("output file must use .c extension");
            if (!writeTextFile(options.output,generated)) throw CompileError("cannot write '"+options.output.string()+"'");
        } else std::cout<<generated;
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"swan: "<<error.what()<<'\n';
        return 1;
    }
}
}
