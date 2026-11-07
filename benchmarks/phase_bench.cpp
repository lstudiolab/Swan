#include "swan/CodeGen/CGenerator.hpp"
#include "swan/Lex/Lexer.hpp"
#include "swan/Parse/Parser.hpp"
#include "swan/Sema/Sema.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using Samples = std::vector<double>;

double milliseconds(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

double median(Samples values) {
    std::sort(values.begin(), values.end());
    const auto middle = values.size() / 2;
    return values.size() % 2 ? values[middle]
                             : (values[middle - 1] + values[middle]) / 2.0;
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 2;
    const std::string source((std::istreambuf_iterator<char>(input)), {});
    constexpr int repeats = 9;
    Samples lex, parse, sema, codegen, total;
    lex.reserve(repeats);
    parse.reserve(repeats);
    sema.reserve(repeats);
    codegen.reserve(repeats);
    total.reserve(repeats);

    for (int run = 0; run < repeats; ++run) {
        const auto allStart = Clock::now();
        auto start = Clock::now();
        auto tokens = swan::Lexer(source).scan();
        auto end = Clock::now();
        lex.push_back(milliseconds(start, end));

        start = Clock::now();
        auto program = swan::Parser(std::move(tokens)).parse();
        end = Clock::now();
        parse.push_back(milliseconds(start, end));

        start = Clock::now();
        swan::Sema(program).analyze();
        end = Clock::now();
        sema.push_back(milliseconds(start, end));

        start = Clock::now();
        auto generated = swan::CGenerator(program).generate();
        end = Clock::now();
        codegen.push_back(milliseconds(start, end));
        total.push_back(milliseconds(allStart, end));
        if (generated.empty()) return 3;
    }

    std::cout << median(lex) << ' ' << median(parse) << ' '
              << median(sema) << ' ' << median(codegen) << ' '
              << median(total) << '\n';
}
