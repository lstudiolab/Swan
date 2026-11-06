#pragma once

#include "Swan/Frontend/Token.hpp"

#include <string>
#include <utility>

namespace swan {

enum class DiagnosticSeverity {
    Warning,
    Error
};

struct SemaDiagnostic {
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    SourceLocation location;
    std::string code;
    std::string message;

    SemaDiagnostic(SourceLocation location, std::string code, std::string message,
                   DiagnosticSeverity severity = DiagnosticSeverity::Error)
        : severity(severity), location(location), code(std::move(code)),
          message(std::move(message)) {}
};

} // namespace swan
