#include "swan/codegen.hpp"
#include "swan/lexer.hpp"
#include "swan/parser.hpp"
#include "swan/semantic.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define SWAN_POPEN _popen
#define SWAN_PCLOSE _pclose
#else
#define SWAN_POPEN popen
#define SWAN_PCLOSE pclose
#include <sys/wait.h>
#endif

namespace {
std::string shellQuote(const std::string& value) {
#ifdef _WIN32
  std::string result = "\"";
  for (char c : value) {
    if (c == '"') result += '\\';
    result += c;
  }
  result += '"';
  return result;
#else
  std::string result = "'";
  for (char c : value) {
    if (c == '\'') result += "'\\''";
    else result += c;
  }
  result += "'";
  return result;
#endif
}

int compileWithClang(const std::string& clang, const std::string& output,
                     const std::string& source, bool objectOnly,
                     const std::string& optimization, bool debug) {
  std::string command = shellQuote(clang) + " -x c -std=c11 ";
  if (objectOnly) command += "-c ";
  command += optimization + " ";
  if (debug) command += "-g ";
  command += "-o " + shellQuote(output) + " -";
  FILE* pipe = SWAN_POPEN(command.c_str(), "w");
  if (!pipe) {
    std::cerr << "swanc: unable to start Clang; check --clang path\n";
    return 127;
  }
  const std::size_t written = std::fwrite(source.data(), 1, source.size(), pipe);
  const int closeStatus = SWAN_PCLOSE(pipe);
  if (written != source.size()) {
    std::cerr << "swanc: failed to send generated source to Clang\n";
    return 1;
  }
#ifdef _WIN32
  return closeStatus == 0 ? 0 : (closeStatus == -1 ? 127 : closeStatus);
#else
  if (closeStatus == -1) return 127;
  if (WIFEXITED(closeStatus)) return WEXITSTATUS(closeStatus);
  return 128;
#endif
}

void usage() {
  std::cout
      << "Swan compiler\n"
      << "usage: swanc [options] file.swan\n\n"
      << "  -o <path>       output executable (default: a.out)\n"
      << "  -c              emit an object file instead of linking\n"
      << "  --check         run frontend checks only\n"
      << "  --clang <path>  Clang executable (default: clang)\n"
      << "  -O0 | -O2 | -O3 optimization level\n"
      << "  -g              include debug information\n"
      << "  -h, --help      show this help\n";
}
} // namespace

int main(int argc, char** argv) {
  std::string input;
  std::string output = "a.out";
  std::string clang = "clang";
  std::string optimization = "-O0";
  bool objectOnly = false;
  bool checkOnly = false;
  bool debug = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      usage();
      return 0;
    } else if (arg == "-c") {
      objectOnly = true;
    } else if (arg == "--check") {
      checkOnly = true;
    } else if (arg == "-g") {
      debug = true;
    } else if (arg == "-O0" || arg == "-O2" || arg == "-O3") {
      optimization = arg;
    } else if ((arg == "-o" || arg == "--clang") && i + 1 < argc) {
      const std::string value = argv[++i];
      if (arg == "-o") output = value;
      else clang = value;
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "swanc: unknown option: " << arg << "\n";
      usage();
      return 2;
    } else if (input.empty()) {
      input = arg;
    } else {
      std::cerr << "swanc: this compiler invocation accepts one .swan file; "
                   "compile each source to an object and link them separately\n";
      return 2;
    }
  }

  if (input.empty()) {
    usage();
    return 2;
  }
  if (std::filesystem::path(input).extension() != ".swan") {
    std::cerr << "swanc: expected a .swan source file: " << input << "\n";
    return 2;
  }

  std::ifstream file(input, std::ios::binary);
  if (!file) {
    std::cerr << "swanc: cannot read " << input << "\n";
    return 2;
  }
  const std::string source((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

  swan::Lexer lexer(source);
  auto tokens = lexer.scan();
  for (const auto& error : lexer.errors())
    std::cerr << input << ": " << error << "\n";
  if (!lexer.errors().empty()) return 1;

  swan::Parser parser(std::move(tokens));
  auto module = parser.parse();
  for (const auto& error : parser.errors())
    std::cerr << input << ": " << error << "\n";
  if (!parser.errors().empty()) return 1;

  swan::SemanticAnalyzer semantics;
  if (!semantics.analyze(module)) {
    for (const auto& error : semantics.errors())
      std::cerr << input << ": " << error << "\n";
    return 1;
  }

  if (checkOnly) {
    std::cout << "swanc: checks passed: " << input << "\n";
    return 0;
  }

  if (output == "a.out" && objectOnly) {
    output = std::filesystem::path(input).stem().string() + ".o";
  } else if (output == "a.out" && !objectOnly) {
    output = std::filesystem::path(input).stem().string();
  }

  swan::CEmitter emitter;
  const std::string generatedSource = emitter.emit(module);
  const int result = compileWithClang(clang, output, generatedSource,
                                      objectOnly, optimization, debug);
  if (result != 0) {
    std::cerr << "swanc: Clang compilation failed (exit " << result << ")\n";
    return result;
  }
  std::cout << "swanc: " << input << " -> " << output << "\n";
  return 0;
}
