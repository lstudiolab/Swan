#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
INCLUDE="$ROOT/compiler/include"
CXX=${CXX:-c++}

find "$INCLUDE/swan" -type f -name '*.hpp' -print | sort |
while IFS= read -r header; do
    relative=${header#"$INCLUDE"/}
    printf '#include "%s"\nint main() { return 0; }\n' "$relative" |
        "$CXX" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
            -I"$INCLUDE" -x c++ -fsyntax-only -
done

printf 'PASS: standalone compiler headers\n'
