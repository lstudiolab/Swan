#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
INCLUDE="$ROOT/compiler/include"
CXX=${CXX:-c++}

CPP_COUNT=$(find "$ROOT" -type f -name '*.cpp' | wc -l | tr -d ' ')
if [ "$CPP_COUNT" -ne 1 ] || [ ! -f "$ROOT/main.cpp" ]; then
    echo "FAIL: the compiler must have exactly one C++ translation unit: main.cpp" >&2
    exit 1
fi

find "$INCLUDE/swan" -type f -name '*.hpp' -print | sort |
while IFS= read -r header; do
    relative=${header#"$INCLUDE"/}
    printf '#include "%s"\nint main() { return 0; }\n' "$relative" |
        "$CXX" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
            -I"$INCLUDE" -x c++ -fsyntax-only -
done

printf '#include "swan/Parse/Parser.hpp"\n#include <vector>\nint main() { swan::Parser parser(std::vector<swan::Token>{}); return parser.parse().empty() ? 0 : 1; }\n' |
    "$CXX" -std=c++20 -Wall -Wextra -Wpedantic -Werror -I"$INCLUDE" -x c++ - -o "$ROOT/build/parser-empty"
"$ROOT/build/parser-empty"

printf 'PASS: one translation unit, standalone headers, and empty parser input\n'
