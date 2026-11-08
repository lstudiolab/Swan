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
cc -x assembler "$TMP/native.s" -o "$TMP/native"
test "$("$TMP/native")" = "91"
if "$SWAN" "$TMP/native.swan" --target aarch64 --emit-asm -o "$TMP/unsupported.s" 2>"$TMP/unsupported.err"; then
    echo "FAIL: unimplemented AArch64 target was accepted" >&2
    exit 1
fi
grep -q "target 'aarch64' is not implemented" "$TMP/unsupported.err"
printf 'PASS: Swan-owned x86-64 assembly and execution\n'
printf 'PASS: unimplemented architecture targets are rejected\n'
