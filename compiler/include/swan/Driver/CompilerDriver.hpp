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

namespace swan::driver {
inline std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    std::string q = "\"";
    for (char c : value) { if (c == '"') q += "\\\\"; q += c; }
    return q + "\"";
#else
    std::string q = "'";
    for (char c : value) { if (c == '\'') q += "'\\''"; else q += c; }
    return q + "'";
#endif
}
inline int exitCode(int status) {
#ifdef _WIN32
    return status;
#else
    if (status == -1) return 1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
#endif
}
inline std::filesystem::path temporaryPath(const std::string& ext) {
    auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("swan-" + std::to_string(stamp) + ext);
}
inline std::string targetTriple(const std::string& target) {
    if (target == "native") return {};
    if (target == "x86_64" || target == "x86-64") return "x86_64-unknown-linux-gnu";
    if (target == "aarch64" || target == "arm64") return "aarch64-unknown-linux-gnu";
    if (target == "armv7") return "armv7-unknown-linux-gnueabihf";
    if (target == "riscv64") return "riscv64-unknown-linux-gnu";
    if (target == "wasm32") return "wasm32-unknown-unknown";
    if (target.find('-') != std::string::npos) return target;
    throw CompileError("unsupported target '" + target + "'; pass a valid LLVM target triple");
}
inline int compileLLVM(const std::string& ir, const std::filesystem::path& output,
                       const std::string& target, bool assembly, bool object, bool execute,
                       bool fastCompile, bool optimize, const std::string& cpu, const std::string& features) {
    const auto irFile = temporaryPath(".ll");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code e; std::filesystem::remove(path, e); } } cleanup{irFile};
    if (!writeTextFile(irFile, ir)) throw CompileError("cannot write temporary LLVM IR");
    const char* configured = std::getenv("SWAN_CLANG");
    const std::string clang = configured && *configured ? configured : "clang";
    const auto triple = targetTriple(target);
    std::string command = shellQuote(clang);
    if (!triple.empty()) command += " -target " + shellQuote(triple);
    command += fastCompile ? " -O0 -fno-addrsig" : optimize ? " -O3 -fno-addrsig" : " -O2 -fno-addrsig";
    if (!cpu.empty()) command += " -mcpu=" + shellQuote(cpu);
    else if (target == "native") command += " -march=native";
    if (!features.empty()) {
        std::size_t start = 0;
        while (start < features.size()) {
            const auto end = features.find(',', start);
            const auto feature = features.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!feature.empty()) command += " -Xclang -target-feature -Xclang " + shellQuote(feature);
            if (end == std::string::npos) break;
            start = end + 1;
        }
    }
    command += " -Wno-override-module -x ir " + shellQuote(irFile.string());
    command += assembly ? " -S -o " : object ? " -c -o " : " -o ";
    command += shellQuote(output.string());
    const int status = exitCode(std::system(command.c_str()));
    if (status != 0) { std::error_code e; std::filesystem::remove(output, e); return status; }
    if (assembly || !execute) return 0;
#ifdef _WIN32
    const std::string exe = output.string();
    const char* args[] = {exe.c_str(), nullptr};
    return _spawnv(_P_WAIT, exe.c_str(), args);
#else
    return exitCode(std::system(shellQuote(output.string()).c_str()));
#endif
}
} // namespace swan::driver

namespace swan {
inline int runCompiler(int argc, char** argv) {
    driver::CommandLine options;
    if (!driver::parseCommandLine(argc, argv, options)) { driver::printHelp(); return 2; }
    if (options.showVersion) { std::cout << "Swan " << versionString() << '\n'; return 0; }
    if (options.showHelp) { driver::printHelp(); return 0; }
    try {
        if (options.input.extension() != ".swan") throw CompileError("input file must use .swan extension");
        std::string source;
        if (!readTextFile(options.input, source)) throw CompileError("cannot open '" + options.input.string() + "'");
        const bool assembly = options.emitAssembly || (options.hasOutput && options.output.extension() == ".s");
        const bool object = options.emitObject || (options.hasOutput && (options.output.extension() == ".o" || options.output.extension() == ".obj"));
        if (assembly && object) throw CompileError("choose either --emit-asm or --emit-obj");
        const bool execute = options.action != driver::Action::Build && !assembly && !object;
        if (!assembly && !object && options.target != "native")
            throw CompileError("cross-target executable linking requires a target sysroot; use --emit-asm");
        auto output = options.hasOutput ? options.output :
            assembly ? std::filesystem::path(options.input.stem().string() + ".s") :
            object ? std::filesystem::path(options.input.stem().string() + ".o") :
            execute ? driver::temporaryPath(
#ifdef _WIN32
                ".exe"
#else
                ""
#endif
            ) : std::filesystem::path(options.input.stem().string()
#ifdef _WIN32
                + ".exe"
#endif
            );
        const char* envFast = std::getenv("SWAN_FAST_COMPILE");
        const bool fast = options.fastCompile || (envFast && *envFast && std::string(envFast) != "0");
        const auto ir = Compiler().compileIR(source, options.target);
        const int status = driver::compileLLVM(ir, output, options.target, assembly, object, execute, fast, options.optimize, options.cpu, options.features);
        if (!options.hasOutput && !assembly && !object) { std::error_code e; std::filesystem::remove(output, e); }
        if (!execute && !assembly && !object && status == 0) std::cout << "Built " << output.string() << '\n';
        return status;
    } catch (const std::exception& e) {
        std::cerr << "swan: " << e.what() << '\n';
        return 1;
    }
}
} // namespace swan
