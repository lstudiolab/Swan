#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CXX=${CXX:-c++}
mkdir -p "$ROOT/build"
"$CXX" -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" "$ROOT/main.cpp" -o "$ROOT/build/swan"
printf 'Built Swan compiler: %s\n' "$ROOT/build/swan"
printf 'Code generation: Swan-owned x86-64 assembly backend\n'
