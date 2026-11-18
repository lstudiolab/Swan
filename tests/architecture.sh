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

printf 'PASS: Swan supports exactly x86-64, AArch64/ARM64, and WebAssembly 64-bit\n'
printf 'PASS: unsupported architectures and arbitrary LLVM target triples are rejected\n'
