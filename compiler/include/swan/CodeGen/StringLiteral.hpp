#pragma once
#include <string>
#include <string_view>

namespace swan {
inline void appendEscapedCString(std::string& output, std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const unsigned char ch : value) {
        switch (ch) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (ch < 32 || ch >= 127) {
                output += "\\x";
                output.push_back(hex[ch >> 4]);
                output.push_back(hex[ch & 15]);
                output += "\"\"";
            } else {
                output.push_back(static_cast<char>(ch));
            }
        }
    }
    output.push_back('"');
}

inline std::string escapeCString(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 2);
    appendEscapedCString(result, value);
    return result;
}
}
