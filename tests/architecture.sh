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
out
fn main() -> int:
    let values = array(8)
    var i = 0
    while i < 8
        values[i] = i * 2
        i = i + 1
    log sum(values[7])
    return 0
out
SWAN
"$SWAN" "$TMP/native.swan" --target x86_64 --emit-asm -o "$TMP/native.s"
cc -x assembler "$TMP/native.s" -o "$TMP/native"
test "$("$TMP/native")" = "91"
cat > "$TMP/scalar.swan" <<'SWAN'
fn twice(x: int) -> int:
    return x * 2
out
fn main() -> int:
    return twice(21)
out
SWAN
"$SWAN" "$TMP/scalar.swan" --target x86_64 --emit-asm -o "$TMP/scalar.s"
grep -q 'imul rax, 2' "$TMP/scalar.s"
if grep -q 'mov rax, 2$' "$TMP/scalar.s"; then
    echo "FAIL: embedded scalar constant was materialized in a stack slot" >&2
    exit 1
fi
cc -x assembler "$TMP/scalar.s" -o "$TMP/scalar"
set +e
"$TMP/scalar"
scalar_status=$?
set -e
test "$scalar_status" -eq 42
"$SWAN" "$TMP/scalar.swan" --target aarch64 --emit-asm -o "$TMP/aarch64.s"
grep -q "stp x29, x30" "$TMP/aarch64.s"
"$SWAN" "$TMP/scalar.swan" --target riscv64 --emit-asm -o "$TMP/riscv64.s"
grep -q "addi sp, sp" "$TMP/riscv64.s"
"$SWAN" --list-targets > "$TMP/targets.txt"
grep -q "x86_64.*implemented" "$TMP/targets.txt"
grep -q "aarch64.*implemented" "$TMP/targets.txt"
grep -q "riscv64.*implemented" "$TMP/targets.txt"
grep -q "wasm32.*implemented" "$TMP/targets.txt"
grep -q "wasm64.*implemented" "$TMP/targets.txt"
grep -q "armv7.*implemented" "$TMP/targets.txt"
if "$SWAN" "$TMP/native.swan" --target madeup 2>"$TMP/unknown.err"; then
    echo "FAIL: unknown target was accepted" >&2
    exit 1
fi
grep -q "unknown target 'madeup'" "$TMP/unknown.err"
printf 'PASS: Swan-owned x86-64 assembly and execution\n'
printf 'PASS: target registry reports implemented and planned backends\n'
 "$TMP/scalar.s"; then
    echo "FAIL: embedded scalar constant was materialized in a stack slot" >&2
    exit 1
fi
cc -x assembler "$TMP/scalar.s" -o "$TMP/scalar"
set +e
"$TMP/scalar"
scalar_status=$?
set -e
test "$scalar_status" -eq 42
"$SWAN" "$TMP/scalar.swan" --target aarch64 --emit-asm -o "$TMP/aarch64.s"
grep -q "stp x29, x30" "$TMP/aarch64.s"
"$SWAN" "$TMP/scalar.swan" --target riscv64 --emit-asm -o "$TMP/riscv64.s"
grep -q "addi sp, sp" "$TMP/riscv64.s"
"$SWAN" --list-targets > "$TMP/targets.txt"
grep -q "x86_64.*implemented" "$TMP/targets.txt"
grep -q "aarch64.*implemented" "$TMP/targets.txt"
grep -q "riscv64.*implemented" "$TMP/targets.txt"
grep -q "wasm32.*implemented" "$TMP/targets.txt"
grep -q "wasm64.*implemented" "$TMP/targets.txt"
grep -q "armv7.*implemented" "$TMP/targets.txt"
if "$SWAN" "$TMP/native.swan" --target madeup 2>"$TMP/unknown.err"; then
    echo "FAIL: unknown target was accepted" >&2
    exit 1
fi
grep -q "unknown target 'madeup'" "$TMP/unknown.err"
printf 'PASS: Swan-owned x86-64 assembly and execution\n'
printf 'PASS: target registry reports implemented and planned backends\n'
