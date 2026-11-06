#pragma once
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
namespace swan {
inline bool readTextFile(const std::filesystem::path& path, std::string& contents) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    contents.assign(std::istreambuf_iterator<char>(input), {});
    return input.good() || input.eof();
}
inline bool writeTextFile(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output << contents;
    return static_cast<bool>(output);
}
}
