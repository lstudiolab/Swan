#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
    "$ROOT/tests/ir_verifier.c" -o "$ROOT/build/ir-verifier-test"
"$ROOT/build/ir-verifier-test"
