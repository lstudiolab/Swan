#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/ir-codegen-phi"
mkdir -p "$TMP"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
    "$ROOT/tests/ir_codegen_phi.c" -o "$TMP/generate"
"$TMP/generate" "$TMP/program.s"
cc -x assembler "$TMP/program.s" -o "$TMP/program"
actual=$("$TMP/program")
if [ "$actual" != "21" ]; then
    echo "FAIL: phi swap produced '$actual', expected '21'" >&2
    exit 1
fi
printf 'PASS: x86-64 backend preserves parallel phi copies across loop backedges\n'
