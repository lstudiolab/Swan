#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/targets"
mkdir -p "$TMP"
cat > "$TMP/native.swan" <<'SWAN'
fn sum(n: int) -> int:
    var total = 0
    var i = 0
    while i < n
        total = total + i
        i = i + 1
    return total
fn main() -> int:
    let values = array(8)
    var i = 0
    while i < 8
        values[i] = i * 2
        i = i + 1
    log sum(values[7])
    return 0
SWAN
"$SWAN" "$TMP/native.swan" --target x86_64 --emit-asm -o "$TMP/native.s"
test -s "$TMP/native.s"
grep -q '^\.intel_syntax noprefix' "$TMP/native.s"
cc -c "$TMP/native.s" -o "$TMP/native.o"
test -s "$TMP/native.o"
"$SWAN" "$TMP/native.swan" --target x86_64 --emit-obj -o "$TMP/native-object.o"
test -s "$TMP/native-object.o"
"$SWAN" build "$TMP/native.swan" -o "$TMP/native"
test "$("$TMP/native")" = "91"
"$SWAN" "$TMP/native.swan" --target aarch64 --emit-asm -o "$TMP/aarch64.s"
test -s "$TMP/aarch64.s"
grep -q '^.global main' "$TMP/aarch64.s"
aarch64-linux-gnu-as "$TMP/aarch64.s" -o "$TMP/aarch64.o"
test -s "$TMP/aarch64.o"
"$SWAN" "$TMP/native.swan" --target aarch64 --emit-obj -o "$TMP/aarch64-driver.o"
test -s "$TMP/aarch64-driver.o"
if "$SWAN" "$TMP/native.swan" --target armv7 --emit-asm -o "$TMP/not-supported.s" 2>"$TMP/unsupported.err"; then
    echo 'FAIL: unimplemented architecture was accepted' >&2
    exit 1
fi
grep -q "Swan-owned backend" "$TMP/unsupported.err"
printf 'PASS: Swan-owned x86-64 and AArch64 assembly generation\n'
printf 'PASS: native and AArch64 object generation plus native linking\n'
printf 'PASS: target selection rejects backends not yet implemented\n'
