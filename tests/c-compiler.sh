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
grep -q 'add qword ptr' "$TMP/loop-x86_64.s"
"$SWAN" "$TMP/loop.swan" --target aarch64 --emit-asm -o "$TMP/loop-aarch64.s"
grep -q 'add x10, x10, #1' "$TMP/loop-aarch64.s"
"$SWAN" "$TMP/loop.swan" --target armv7 --emit-asm -o "$TMP/loop-armv7.s"
grep -q 'add r0, r0, #1' "$TMP/loop-armv7.s"
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
cat > "$TMP/loop-control.swan" <<'SWAN'
fn main() -> int:
    for i in 0..5
        continue
        log "unreachable"
    log "continued"
    for j in 0..5
        break
        log "unreachable"
    log "broken"
    return 0
SWAN
"$SWAN" build "$TMP/loop-control.swan" --target native -o "$TMP/loop-control"
test "$("$TMP/loop-control")" = "continued
broken"
cat > "$TMP/bad-break.swan" <<'SWAN'
fn main():
    break
SWAN
if "$SWAN" build "$TMP/bad-break.swan" -o "$TMP/bad-break" 2>/dev/null; then
  echo "FAIL: break outside loop accepted" >&2
  exit 1
fi
printf 'PASS: native nested loops, loop variables, break/continue, and architecture assembly\n'
