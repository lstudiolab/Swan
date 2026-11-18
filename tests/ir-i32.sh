#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/ir-i32"
mkdir -p "$TMP"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
    "$ROOT/tests/ir_i32.c" -o "$TMP/generate"
"$TMP/generate" "$TMP/program.s" "$TMP/program.aarch64.s"
clang --target=aarch64-unknown-linux-gnu -c -x assembler "$TMP/program.aarch64.s" -o "$TMP/program.aarch64.o"
grep -q "sxtw x9, w0" "$TMP/program.aarch64.s"
grep -q "sdiv w11, w9, w10" "$TMP/program.aarch64.s"
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
