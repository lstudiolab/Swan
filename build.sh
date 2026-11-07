#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CXX=${CXX:-c++}
if ! command -v "${SWAN_CLANG:-clang}" >/dev/null 2>&1; then
  echo "Swan requires Clang/LLVM for native and cross-target machine-code generation." >&2
  exit 1
fi
mkdir -p "$ROOT/build"
"$CXX" -std=c++20 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
  "$ROOT/main.cpp" -o "$ROOT/build/swan"
printf 'Built Swan native compiler: %s\n' "$ROOT/build/swan"
printf 'LLVM backend: %s\n' "${SWAN_CLANG:-clang}"
