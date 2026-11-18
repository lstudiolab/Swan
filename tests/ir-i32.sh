#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/ir-i32"
mkdir -p "$TMP"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
    "$ROOT/tests/ir_i32.c" -o "$TMP/generate"
"$TMP/generate" "$TMP/program.s"
cc -x assembler "$TMP/program.s" -o "$TMP/program"
set +e
"$TMP/program"
status=$?
set -e
if [ "$status" -ne 33 ]; then
    echo "FAIL: x86-64 i32 arithmetic returned '$status', expected 33" >&2
    exit 1
fi
printf 'PASS: x86-64 IR i32 arithmetic, division, shifts, calls, and ABI arguments\n'
