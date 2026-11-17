#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
SANITIZER_FLAGS=""
if [ "${SWAN_SANITIZE:-0}" = "1" ]; then
    SANITIZER_FLAGS="-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
fi
mkdir -p "$ROOT/build"

# Build the single C11 production compiler directly; there is no bootstrap stage or second compiler.
rm -f "$ROOT/build/swan-bootstrap" \
      "$ROOT/build/sil_runtime.swan" \
      "$ROOT/build/sil_runtime.s" \
      "$ROOT/build/sil_runtime.o"

"$CC" -std=c11 -O2 -DNDEBUG $SANITIZER_FLAGS -DSWAN_USE_MONOTONIC_CLOCK \
    -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration \
    -Werror=return-type -Werror=incompatible-pointer-types \
    -I"$ROOT/compiler/include" "$ROOT/compiler/tools/swanc/main.c" \
    -o "$ROOT/build/swanc"
ln -sf swanc "$ROOT/build/swan"

printf 'Built Swan production compiler: %s\n' "$ROOT/build/swanc"
printf 'Compiler implementation: C11 (single production compiler)\n'
printf 'Bootstrap: none\n'
printf 'Pipeline: frontend -> SVM typed IR -> optimizer -> Swan target backend\n'
