#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CC=${CC:-cc}
COUNT=$(find "$ROOT/compiler" -type f -name '*.c' | wc -l | tr -d ' ')
DRIVER="$ROOT/compiler/tools/swanc/main.c"
if [ "$COUNT" -ne 1 ] || [ ! -f "$DRIVER" ] || [ ! -f "$ROOT/main.c" ]; then
    echo "FAIL: expected one compiler translation unit at compiler/tools/swanc/main.c and the root compatibility entry point" >&2
    exit 1
fi
CPP_FILES=$(find "$ROOT" -type f | grep -E '\.(cpp|cc|cxx|hpp)$' || true)
if [ -n "$CPP_FILES" ]; then
    echo "FAIL: C++ source/header files remain in the compiler implementation:" >&2
    echo "$CPP_FILES" >&2
    exit 1
fi
find "$ROOT/compiler/include/swan" -type f -name '*.h' -print | sort |
while IFS= read -r header; do
    relative=${header#"$ROOT/compiler/include/"}
    printf '#include "%s"\nint main(void) { return 0; }\n' "$relative" |
        "$CC" -std=c11 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" -x c -fsyntax-only -
done
printf 'PASS: one C translation unit and standalone C headers\n'
