#pragma once
#include <string>

namespace swan {
inline void appendCString(std::string& result, const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    result.reserve(result.size() + value.size() + 2);
    result.push_back('"');
    for (unsigned char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (ch < 32 || ch >= 127) {
                result += "\\x";
                result.push_back(hex[ch >> 4]);
                result.push_back(hex[ch & 15]);
                result += "\"\"";
            } else {
                result.push_back(static_cast<char>(ch));
            }
        }
    }
    result.push_back('"');
}

inline std::string escapeCString(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 2);
    appendCString(result, value);
    return result;
}
}
