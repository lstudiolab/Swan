#include "swan/Compiler/Compiler.hpp"
#include "swan/CodeGen/CGenerator.hpp"
#include "swan/Lex/Lexer.hpp"
#include "swan/Parse/Parser.hpp"
#include "swan/Sema/Sema.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

template <class Function>
double medianMilliseconds(Function&& function) {
    for (int i = 0; i < 2; ++i) function();
    std::vector<double> samples;
    samples.reserve(7);
    for (int i = 0; i < 7; ++i) {
        const auto start = Clock::now();
        function();
        const auto finish = Clock::now();
        samples.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

double parseMilliseconds(const std::string& source) {
    std::vector<double> samples;
    samples.reserve(7);
    for (int i = 0; i < 9; ++i) {
        auto tokens = swan::Lexer(source).scan();
        const auto start = Clock::now();
        auto program = swan::Parser(std::move(tokens)).parse();
        const auto finish = Clock::now();
        (void)program;
        if (i >= 2)
            samples.push_back(std::chrono::duration<double, std::milli>(finish - start).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2];
}

std::string makeSource(std::size_t statements) {
    std::string source;
    source.reserve(statements * 40);
    source += "fn workload():\n";
    for (std::size_t i = 0; i < statements; ++i) {
        source += "    var value";
        source += std::to_string(i);
        source += ": int = ";
        source += std::to_string(i % 1000);
        source += '\n';
    }
    source += "    return value";
    source += std::to_string(statements - 1);
    source += '\n';
    return source;
}

swan::Program parseSource(const std::string& source) {
    auto tokens = swan::Lexer(source).scan();
    return swan::Parser(std::move(tokens)).parse();
}
} // namespace

int main(int argc, char** argv) {
    const std::filesystem::path outputDirectory = argc > 1 ? argv[1] : "build";
    std::filesystem::create_directories(outputDirectory);

    std::cout << "statements,source_bytes,tokens,lex_ms,parse_ms,sema_ms,codegen_ms,frontend_ms,c_compile_ms,swan_total_ms,generated_c_bytes\n";
    for (const std::size_t count : {128u, 512u, 2048u, 8192u}) {
        const std::string source = makeSource(count);
        const auto tokenCount = swan::Lexer(source).scan().size();

        const double lexMs = medianMilliseconds([&] {
            auto tokens = swan::Lexer(source).scan();
            (void)tokens;
        });

        const double parseMs = parseMilliseconds(source);

        const auto semanticProgram = parseSource(source);
        const double semaMs = medianMilliseconds([&] {
            swan::Sema semantic(semanticProgram);
            semantic.analyze();
        });

        swan::Sema validation(semanticProgram);
        validation.analyze();

        std::string generated;
        const double codegenMs = medianMilliseconds([&] {
            generated = swan::CGenerator(semanticProgram).generate();
        });

        const double frontendMs = medianMilliseconds([&] {
            generated = swan::Compiler().compile(source);
        });

        const auto generatedPath = outputDirectory / ("pipeline-" + std::to_string(count) + ".c");
        std::ofstream output(generatedPath, std::ios::binary);
        output.write(generated.data(), static_cast<std::streamsize>(generated.size()));
        if (!output) {
            std::cerr << "failed to write " << generatedPath << '\n';
            return 1;
        }

        output.close();
        const auto objectPath = outputDirectory / ("pipeline-" + std::to_string(count) + ".o");
        const std::string compileCommand = "cc -std=c11 -O2 -c \"" +
            generatedPath.string() + "\" -o \"" + objectPath.string() + "\"";
        const double cCompileMs = medianMilliseconds([&] {
            if (std::system(compileCommand.c_str()) != 0) {
                std::cerr << "C compiler failed for " << generatedPath << '\n';
                std::exit(1);
            }
        });

        std::cout << count << ',' << source.size() << ',' << tokenCount << ','
                  << std::fixed << std::setprecision(3)
                  << lexMs << ',' << parseMs << ',' << semaMs << ','
                  << codegenMs << ',' << frontendMs << ',' << cCompileMs << ','
                  << (frontendMs + cCompileMs) << ',' << generated.size() << '\n';
    }
    return 0;
}
