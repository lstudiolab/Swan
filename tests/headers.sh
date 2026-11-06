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

printf 'PASS: one translation unit and standalone compiler headers\n'
