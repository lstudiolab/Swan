#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CXX=${CXX:-c++}
CC=${CC:-cc}
mkdir -p "$ROOT/build"
"$CXX" -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
  "$ROOT/main.cpp" -o "$ROOT/build/swan"
"$CC" -std=c11 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  "$ROOT/compiler-c/swan.c" -o "$ROOT/build/swan-c"
printf 'Built Swan compiler: %s\n' "$ROOT/build/swan"
printf 'Built standalone C subset compiler: %s\n' "$ROOT/build/swan-c"
