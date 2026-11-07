#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
mkdir -p "$ROOT/build"
"$CC" -std=c11 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" "$ROOT/main.c" -o "$ROOT/build/swan"
printf 'Built Swan C compiler: %s\n' "$ROOT/build/swan"
printf 'Implementation: C11 headers; one translation unit (main.c)\n'
printf 'Code generation: Swan-owned x86-64 assembly backend\n'
