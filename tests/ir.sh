#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" "$ROOT/tests/ir.c" -o "$ROOT/build/ir-test"
"$ROOT/build/ir-test"
printf 'PASS: typed SSA IR construction and verifier\n'
