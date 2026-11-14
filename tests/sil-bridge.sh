#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/test/sil-bridge"
mkdir -p "$TMP"
test -f "$ROOT/build/sil_runtime.o"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
    "$ROOT/tests/sil_bridge.c" "$ROOT/build/sil_runtime.o" -o "$TMP/sil-bridge"
"$TMP/sil-bridge"
