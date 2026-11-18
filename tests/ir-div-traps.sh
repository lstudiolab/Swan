#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/ir-div-traps"
mkdir -p "$TMP"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
    "$ROOT/tests/ir_div_trap.c" -o "$TMP/generate"
for mode in zero overflow unsigned i32-zero i32-overflow i32-unsigned; do
    "$TMP/generate" "$mode" "$TMP/$mode.s"
    cc -x assembler "$TMP/$mode.s" -o "$TMP/$mode"
    set +e
    "$TMP/$mode" >/dev/null 2>&1
    status=$?
    set -e
    if [ "$status" -eq 0 ]; then
        echo "FAIL: x86-64 IR $mode division did not trap" >&2
        exit 1
    fi
done
printf 'PASS: x86-64 IR division traps on zero and signed overflow\\n'
