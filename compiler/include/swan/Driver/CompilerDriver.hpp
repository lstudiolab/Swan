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
        if (character == '\"') quoted += '\\\\';
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

inline std::string targetTriple(const std::string& target) {
    if (target == "native") return {};
    if (target == "x86_64") return "x86_64-linux-gnu";
    if (target == "aarch64") return "aarch64-linux-gnu";
    if (target == "armv7") return "armv7-linux-gnueabihf";
    if (target == "riscv64") return "riscv64-linux-gnu";
    if (target == "wasm32") return "wasm32-unknown-unknown";
    if (target.find('-') != std::string::npos) return target;
    throw CompileError("unsupported target '" + target +
        "'; use a supported alias or a Clang target triple");
}

inline int compilePortable(const std::string& generated,
                           const std::filesystem::path& output,
                           const std::string& target,
                           bool emitAssembly,
                           bool execute) {
    const auto sourceFile = temporaryPath(".c");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{sourceFile};
    if (!writeTextFile(sourceFile, generated))
        throw CompileError("cannot write temporary portable source");

    if (emitAssembly) {
        const char* configured = std::getenv("SWAN_CLANG");
        const std::string compiler = configured && *configured ? configured : "clang";
        std::string command = shellQuote(compiler);
        const auto triple = targetTriple(target);
        if (!triple.empty()) command += " -target " + shellQuote(triple);
        command += " -O3 -fwrapv -std=c11 -S -x c " + shellQuote(sourceFile.string()) +
            " -o " + shellQuote(output.string());
        return processExitCode(std::system(command.c_str()));
    }

    if (target != "native")
        throw CompileError("cross-target executable linking requires a target sysroot; use --emit-asm");
    const char* configured = std::getenv("CC");
    const std::string compiler = configured && *configured ? configured : "cc";
    const std::string command = shellQuote(compiler) + " -O3 -march=native -fwrapv -std=c11 -x c " +
        shellQuote(sourceFile.string()) + " -o " + shellQuote(output.string());
    const int status = processExitCode(std::system(command.c_str()));
    if (status != 0) {
        std::error_code error;
        std::filesystem::remove(output, error);
        return status;
    }
    if (!execute) return 0;
#ifdef _WIN32
    const std::string executable = output.string();
    const char* arguments[] = {executable.c_str(), nullptr};
    return _spawnv(_P_WAIT, executable.c_str(), arguments);
#else
    return processExitCode(std::system(shellQuote(output.string()).c_str()));
#endif
}

inline int compileNative(const std::string& generated,
                         const std::filesystem::path& output,
                         bool execute) {
    const auto assemblyFile = temporaryPath(".s");
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove(path, error); }
    } cleanup{assemblyFile};

    if (!writeTextFile(assemblyFile, generated))
        throw CompileError("cannot write temporary assembly source");

    const char* configuredCompiler = std::getenv("CC");
    const std::string cCompiler = configuredCompiler && *configuredCompiler
        ? configuredCompiler : "cc";
    const std::string command = shellQuote(cCompiler) + " -x assembler " +
        shellQuote(assemblyFile.string()) + " -o " + shellQuote(output.string());
    const int compileStatus = processExitCode(std::system(command.c_str()));
    if (compileStatus != 0) {
        std::error_code error;
        std::filesystem::remove(output, error);
        return compileStatus;
    }

    if (!execute) return 0;
#ifdef _WIN32
    const std::string executable = output.string();
    const char* arguments[] = {executable.c_str(), nullptr};
    return _spawnv(_P_WAIT, executable.c_str(), arguments);
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

        const bool emitAssembly = options.emitAssembly ||
            (options.hasOutput && options.output.extension() == ".s");
#if defined(__linux__) && defined(__x86_64__)
        constexpr bool directNativeSupported = true;
#else
        constexpr bool directNativeSupported = false;
#endif
        const bool portable = options.optimize || options.target != "native" ||
            !directNativeSupported;
        if (portable) {
            const auto output = options.hasOutput ? options.output
                : std::filesystem::path(options.input.stem().string() + ".s");
            if (emitAssembly) {
                const auto generated = Compiler().compilePortable(source);
                return driver::compilePortable(generated, output, options.target, true, false);
            }
            if (options.target != "native")
                throw CompileError("non-native targets currently require --emit-asm");
            const bool execute = options.action != driver::Action::Build;
            const auto executable = options.hasOutput ? options.output
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
            const int status = driver::compilePortable(
                Compiler().compilePortable(source), executable, options.target, false, execute);
            if (!options.hasOutput) {
                std::error_code error;
                std::filesystem::remove(executable, error);
            }
            if (!execute && status == 0)
                std::cout << "Built " << executable.string() << '\\n';
            return status;
        }

        if (options.target != "native")
            (void)driver::targetTriple(options.target);
        const std::string generated = Compiler().compile(source);
        if (emitAssembly) {
            const auto output = options.hasOutput ? options.output
                : std::filesystem::path(options.input.stem().string() + ".s");
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
        if (!options.hasOutput) {
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