#pragma once
#include "swan/Compiler/Compiler.hpp"
#include "swan/Driver/CommandLine.hpp"
#include "swan/Support/FileSystem.hpp"
#include "swan/Basic/Diagnostic.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>
#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#endif

namespace swan {
namespace driver {
inline std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    std::string quoted = "\"";
    for (char character : value) {
        if (character == '\"') quoted += '\\';
        quoted += character;
    }
    return quoted + "\"";
#else
    std::string quoted = "'";
    for (char character : value) {
        if (character == '\'') quoted += "'\\''";
        else quoted += character;
    }
    return quoted + "'";
#endif
}

inline int processExitCode(int status) {
#ifdef _WIN32
    return status;
#else
    if (status == -1) return 1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
#endif
}

inline std::filesystem::path temporaryPath(const std::string& extension) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
        ("swan-" + std::to_string(stamp) + extension);
}

inline int compileNative(const std::string& generated,
                         const std::filesystem::path& output,
                         bool execute) {
    const auto cFile = temporaryPath(".c");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{cFile};

    if (!writeTextFile(cFile, generated))
        throw CompileError("cannot write temporary C source");

    const char* configuredCompiler = std::getenv("CC");
    const std::string cCompiler = configuredCompiler && *configuredCompiler
        ? configuredCompiler : "cc";
    const std::string command = shellQuote(cCompiler) + " -std=c11 -O2 " +
        shellQuote(cFile.string()) + " -o " + shellQuote(output.string());
    const int compileStatus = processExitCode(std::system(command.c_str()));
    if (compileStatus != 0) {
        std::error_code error;
        std::filesystem::remove(output, error);
        return compileStatus;
    }

    if (!execute) return 0;
#ifdef _WIN32
    return _spawnv(_P_WAIT, output.string().c_str(),
                   std::vector<const char*>{output.string().c_str(), nullptr}.data());
#else
    const std::string runCommand = shellQuote(output.string());
    return processExitCode(std::system(runCommand.c_str()));
#endif
}
} // namespace driver

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
        if (options.emitC || (options.hasOutput && options.output.extension() == ".c")) {
            const auto output = options.hasOutput ? options.output
                : std::filesystem::path(options.input.stem().string() + ".c");
            if (!writeTextFile(output, generated))
                throw CompileError("cannot write '" + output.string() + "'");
            return 0;
        }

        const bool execute = options.action != driver::Action::Build;
        const auto output = options.hasOutput ? options.output
            : execute ? driver::temporaryPath(
#ifdef _WIN32
                ".exe"
#else
                ""
#endif
              )
              : std::filesystem::path(options.input.stem().string()
#ifdef _WIN32
                  + ".exe"
#endif
                );

        const int status = driver::compileNative(generated, output, execute);
        if (execute || !options.hasOutput) {
            std::error_code error;
            std::filesystem::remove(output, error);
        }
        if (!execute && status == 0)
            std::cout << "Built " << output.string() << '\n';
        return status;
    } catch (const std::exception& error) {
        std::cerr << "swan: " << error.what() << '\n';
        return 1;
    }
}
} // namespace swan
