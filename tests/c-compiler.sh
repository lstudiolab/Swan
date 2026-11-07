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
printf 'PASS: C compiler native execution and x86-64/AArch64/ARMv7 assembly emission\n'
