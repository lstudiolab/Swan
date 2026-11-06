#pragma once
#include <sstream>
#include <string>
namespace swan {
inline std::string escapeCString(const std::string& value) {
    std::ostringstream result;
    result << '"';
    for (unsigned char ch : value) {
        switch (ch) {
            case '\\': result << "\\\\"; break;
            case '"': result << "\\\""; break;
            case '\n': result << "\\n"; break;
            case '\r': result << "\\r"; break;
            case '\t': result << "\\t"; break;
            default:
                if (ch < 32 || ch >= 127) {
                    const char* hex = "0123456789abcdef";
                    result << "\\x" << hex[ch >> 4] << hex[ch & 15] << "\"\"";
                } else result << static_cast<char>(ch);
        }
    }
    result << '"';
    return result.str();
}
}
