#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/c-compiler-tests"
mkdir -p "$TMP"
cat > "$TMP/hello.swan" <<'SWAN'
fn main():
    log "Hello from Swan"
    return 0
SWAN
"$SWAN" "$TMP/hello.swan" --target native -o "$TMP/hello"
test "$("$TMP/hello")" = "Hello from Swan"
"$SWAN" "$TMP/hello.swan" --target x86_64 --emit-asm -o "$TMP/x86_64.s"
grep -q '\.globl main' "$TMP/x86_64.s"
"$SWAN" "$TMP/hello.swan" --target aarch64 --emit-asm -o "$TMP/aarch64.s"
grep -q 'global main' "$TMP/aarch64.s"
grep -q 'adrp x0' "$TMP/aarch64.s"
"$SWAN" "$TMP/hello.swan" --target armv7 --emit-asm -o "$TMP/armv7.s"
grep -q 'global main' "$TMP/armv7.s"
grep -q 'ldr r0' "$TMP/armv7.s"
if "$SWAN" "$TMP/hello.swan" --target mips --emit-asm -o "$TMP/bad.s" 2>/dev/null; then
  echo "FAIL: unknown target accepted" >&2
  exit 1
fi
cat > "$TMP/loop.swan" <<'SWAN'
fn main() -> int:
    for i in 0..3
        log "Hello, World!"
    return 0
SWAN
"$SWAN" build "$TMP/loop.swan" --target native -o "$TMP/loop"
test "$("$TMP/loop" | wc -l | tr -d ' ')" = "3"
"$SWAN" "$TMP/loop.swan" --target x86_64 --emit-asm -o "$TMP/loop-x86_64.s"
grep -q 'inc r12' "$TMP/loop-x86_64.s"
"$SWAN" "$TMP/loop.swan" --target aarch64 --emit-asm -o "$TMP/loop-aarch64.s"
grep -q 'add x19, x19, #1' "$TMP/loop-aarch64.s"
"$SWAN" "$TMP/loop.swan" --target armv7 --emit-asm -o "$TMP/loop-armv7.s"
grep -q 'add r4, r4, #1' "$TMP/loop-armv7.s"
cat > "$TMP/loops.swan" <<'SWAN'
fn main() -> int:
    for i in 0..3
        log i
    for outer in 0..2
        for inner in 0..3
            log "nested"
    for skipped in 5..5
        log "must-not-print"
    return 0
SWAN
"$SWAN" build "$TMP/loops.swan" --target native -o "$TMP/loops"
"$TMP/loops" > "$TMP/loops.out"
test "$(sed -n '1,3p' "$TMP/loops.out" | tr '\n' ' ')" = "0 1 2 "
test "$(grep -c '^nested$' "$TMP/loops.out")" = "6"
test "$(grep -c 'must-not-print' "$TMP/loops.out" || true)" = "0"
"$SWAN" "$TMP/loops.swan" --target x86_64 --emit-asm -o "$TMP/loops-x86_64.s"
grep -q 'jge .Lswan_end_' "$TMP/loops-x86_64.s"
"$SWAN" "$TMP/loops.swan" --target aarch64 --emit-asm -o "$TMP/loops-aarch64.s"
grep -q 'b.ge .Lswan_end_' "$TMP/loops-aarch64.s"
"$SWAN" "$TMP/loops.swan" --target armv7 --emit-asm -o "$TMP/loops-armv7.s"
grep -q 'bge .Lswan_end_' "$TMP/loops-armv7.s"
printf 'PASS: native range loops and C compiler execution and x86-64/AArch64/ARMv7 assembly emission\n'
