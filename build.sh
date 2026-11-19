#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
SANITIZER_FLAGS=""
if [ "${BREW_SANITIZE:-0}" = "1" ]; then
    SANITIZER_FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
fi
mkdir -p "$ROOT/build"

# Build the single C11 production compiler directly; there is no bootstrap stage or second compiler.
rm -f "$ROOT/build/brew-bootstrap" \
      "$ROOT/build/sil_runtime.brew" \
      "$ROOT/build/sil_runtime.s" \
      "$ROOT/build/sil_runtime.o"

"$CC" -std=c11 -O2 -DNDEBUG $SANITIZER_FLAGS -DBREW_USE_MONOTONIC_CLOCK \
    -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration \
    -Werror=return-type -Werror=incompatible-pointer-types \
    -I"$ROOT/compiler/include" "$ROOT/compiler/tools/brewc/main.c" \
    -o "$ROOT/build/brewc"
ln -sf brewc "$ROOT/build/brew"

printf 'Built Brew production compiler: %s\n' "$ROOT/build/brewc"
printf 'Compiler implementation: C11 (single production compiler)\n'
printf 'Bootstrap: none\n'
printf 'Pipeline: frontend -> typed BVM IR -> verifier -> optimizer -> Brew target backend (feature coverage is documented in docs/compiler_completion_matrix.md)\n'
