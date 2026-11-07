#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
mkdir -p "$ROOT/build"
"$CC" -std=c11 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror \
  "$ROOT/compiler-c/swan.c" -o "$ROOT/build/swan"
printf 'Built C compiler: %s\n' "$ROOT/build/swan"
