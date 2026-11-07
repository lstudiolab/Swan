#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/c11-tests"
mkdir -p "$TMP"
cat > "$TMP/hello.swan" <<'SWAN'
fn main():
    log "Hello from Swan C"
    return 0
SWAN
"$SWAN" "$TMP/hello.swan" --emit-asm -o "$TMP/hello.s"
cc -x assembler "$TMP/hello.s" -o "$TMP/hello"
test "$("$TMP/hello")" = "Hello from Swan C"
printf 'PASS: C11 frontend emits native x86-64 assembly and string output\n'
cat > "$TMP/arithmetic.swan" <<'SWAN'
fn main():
    let a: int = 40
    var b: int = 2
    b = b + 1
    log a + b
    return 0
SWAN
"$SWAN" "$TMP/arithmetic.swan" --emit-asm -o "$TMP/arithmetic.s"
cc -x assembler "$TMP/arithmetic.s" -o "$TMP/arithmetic"
test "$("$TMP/arithmetic")" = "43"
printf 'PASS: typed locals, mutation, and arithmetic lowering\n'
cat > "$TMP/calls.swan" <<'SWAN'
fn add(a: int, b: int) -> int:
    return a + b
fn main():
    log add(20, 22)
    return 0
SWAN
"$SWAN" "$TMP/calls.swan" --emit-asm -o "$TMP/calls.s"
cc -x assembler "$TMP/calls.s" -o "$TMP/calls"
test "$("$TMP/calls")" = "42"
printf 'PASS: function calls and System V argument registers\n'
"$SWAN" --version | grep -q 'Swan 0.6.0'
printf 'PASS: C compiler version command\n'
