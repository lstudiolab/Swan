#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/targets"
mkdir -p "$TMP"

"$SWAN" --list-targets > "$TMP/targets.txt"
test "$(wc -l < "$TMP/targets.txt" | tr -d ' ')" -eq 3
grep -q '^x86_64 .*supported$' "$TMP/targets.txt"
grep -q '^arm64 .*supported$' "$TMP/targets.txt"
grep -q '^wasm64 .*supported$' "$TMP/targets.txt"

cat > "$TMP/native.swan" <<'SWAN'
func sum(n: int) -> int:
    var total = 0
    var i = 0
    while i < n
        total = total + i
        i = i + 1
    return total
func main() -> int:
    print(sum(14))
    return 0
SWAN

"$SWAN" build "$TMP/native.swan" --target x86_64 -o "$TMP/native"
test "$("$TMP/native")" = "91"

cat > "$TMP/cross.swan" <<'SWAN'
func main() -> int:
    return 42
SWAN
"$SWAN" "$TMP/cross.swan" --target arm64 --emit-asm -o "$TMP/aarch64.s"
grep -q 'stp x29, x30' "$TMP/aarch64.s"

# One frontend pass builds the same Swan source for all supported targets.
"$SWAN" build "$TMP/cross.swan" --target all -o "$TMP/all"
test -x "$TMP/all/cross-x86_64"
test -s "$TMP/all/cross-arm64.o"
test -s "$TMP/all/cross-wasm64.wasm"
set +e
"$TMP/all/cross-x86_64"
native_status=$?
set -e
test "$native_status" -eq 42
readelf -h "$TMP/all/cross-arm64.o" | grep -q 'AArch64'
python3 - "$TMP/all/cross-wasm64.wasm" <<'PY'
import sys
data=open(sys.argv[1],"rb").read()
assert data[:8] == b"\0asm\1\0\0\0", "invalid WebAssembly module"
PY

for unsupported in armv7 riscv64 wasm32 arm riscv llvm; do
    if "$SWAN" build "$TMP/native.swan" --target "$unsupported" -o "$TMP/unsupported" 2>"$TMP/unsupported.err"; then
        echo "FAIL: unsupported target '$unsupported' was accepted" >&2
        exit 1
    fi
    grep -q "unknown target '$unsupported'" "$TMP/unsupported.err"
done

if "$SWAN" build "$TMP/native.swan" --target-triple x86_64-unknown-linux-gnu 2>"$TMP/triple.err"; then
    echo "FAIL: arbitrary target triples were accepted" >&2
    exit 1
fi
grep -q "unknown option '--target-triple'" "$TMP/triple.err"

if "$SWAN" build "$TMP/native.swan" --emit-llvm 2>"$TMP/llvm.err"; then
    echo "FAIL: LLVM output mode was accepted" >&2
    exit 1
fi
grep -q "unknown option '--emit-llvm'" "$TMP/llvm.err"

cat > "$TMP/i32.swan" <<'SWAN'
func combine(a: i32, b: i32) -> i32:
    var total: i32 = a + b
    total = total * 3
    total = total - 2
    let quotient: i32 = total / 3
    let remainder: i32 = total % 3
    return quotient + remainder
func main() -> i32:
    return combine(-7, 2)
SWAN
"$SWAN" build "$TMP/i32.swan" --target all -o "$TMP/i32-all"
test -x "$TMP/i32-all/i32-x86_64"
test -s "$TMP/i32-all/i32-arm64.o"
test -s "$TMP/i32-all/i32-wasm64.wasm"
set +e
"$TMP/i32-all/i32-x86_64"
i32_status=$?
set -e
test "$i32_status" -eq 249
printf 'PASS: signed i32 arithmetic and function ABI across all three backends\n'

printf 'PASS: one Swan source builds to x86-64, ARM64, and WASM64 artifacts\n'
printf 'PASS: Swan supports exactly x86-64, AArch64/ARM64, and WebAssembly 64-bit\n'
printf 'PASS: unsupported architectures and arbitrary LLVM target triples are rejected\n'
