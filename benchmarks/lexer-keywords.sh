#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$ROOT/build"
cc -O3 -std=c11 -Wall -Wextra -Wpedantic -Werror -I"$ROOT/compiler/include" \
  "$ROOT/benchmarks/lexer-keywords.c" -o "$ROOT/build/lexer-keywords"
"$ROOT/build/lexer-keywords"
