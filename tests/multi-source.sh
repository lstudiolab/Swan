#!/usr/bin/env sh
set -eu
set -x
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/multi-source"
rm -rf "$TMP"
mkdir -p "$TMP/project"

cat > "$TMP/project/01_math.swan" <<'SWAN'
export C func swan_answer() -> int:
    return 42
SWAN

cat > "$TMP/project/02_main.swan" <<SWAN
import C "$TMP/project/c_api.h"
func main() -> int:
    return swan_answer() + c_answer(42) - 126
SWAN

cat > "$TMP/project/c_api.h" <<'C'
#include <stdint.h>
int64_t c_answer(int64_t value);
C
cat > "$TMP/project/native.c" <<'C'
#include <stdint.h>
int64_t swan_answer(void);
int64_t c_answer(int64_t value) { return value + swan_answer(); }
C

"$ROOT/build/swanc" build "$TMP/project" -o "$TMP/project-app"
"$TMP/project-app"

"$ROOT/build/swanc" build "$TMP/project" --emit-asm -o "$TMP/assembly"
test -f "$TMP/assembly/01_math.s"
test -f "$TMP/assembly/02_main.s"
test -f "$TMP/assembly/native_0.s"
grep -q 'swan_answer' "$TMP/assembly/01_math.s"
grep -q 'c_answer' "$TMP/assembly/02_main.s"
grep -q 'main' "$TMP/assembly/02_main.s"
grep -q '\.extern swan_answer' "$TMP/assembly/02_main.s"
grep -q '\.extern c_answer' "$TMP/assembly/02_main.s"
grep -q 'c_answer' "$TMP/assembly/native_0.s"

cat > "$TMP/bad.swan" <<'SWAN'
func main() -> int:
    return (
SWAN
if "$ROOT/build/swanc" build "$TMP/bad.swan" -o "$TMP/invalid" 2>"$TMP/diagnostic"; then
    echo "expected malformed source to fail" >&2
    exit 1
fi
grep -F "$TMP/bad.swan:" "$TMP/diagnostic" >/dev/null

cat > "$TMP/bad-name.swan" <<'SWAN'
func main() -> int:
    return missing_value
SWAN
if "$ROOT/build/swanc" build "$TMP/bad-name.swan" -o "$TMP/invalid-name" 2>"$TMP/name-diagnostic"; then
    echo "expected unresolved name to fail" >&2
    exit 1
fi
grep -F "$TMP/bad-name.swan:" "$TMP/name-diagnostic" >/dev/null

cat > "$TMP/warning.swan" <<'SWAN'
func unused_helper() -> i64:
    return 7
func main() -> int:
    return 0
SWAN
"$ROOT/build/swanc" build "$TMP/warning.swan" -o "$TMP/warning" 2>"$TMP/warning-diagnostic"
grep -F "$TMP/warning.swan:" "$TMP/warning-diagnostic" >/dev/null

mkdir -p "$TMP/cross"
cat > "$TMP/cross/01_helper.swan" <<'SWAN'
export func cross_value() -> bool:
    return true
SWAN
cat > "$TMP/cross/02_main.swan" <<'SWAN'
func main() -> i32:
    return cross_value() - 42
SWAN
"$ROOT/build/swanc" build "$TMP/cross" --target aarch64 --emit-asm -o "$TMP/aarch64"
test -f "$TMP/aarch64/01_helper.s"
test -f "$TMP/aarch64/02_main.s"
mkdir -p "$TMP/armv7-src"
cat > "$TMP/armv7-src/01_helper.swan" <<'SWAN'
export func arm_value() -> i32:
    return 42
SWAN
cat > "$TMP/armv7-src/02_main.swan" <<'SWAN'
func main() -> i32:
    return arm_value() - 42
SWAN
"$ROOT/build/swanc" build "$TMP/armv7-src" --target armv7 --emit-asm -o "$TMP/armv7"
test -f "$TMP/armv7/01_helper.s"
test -f "$TMP/armv7/02_main.s"
"$ROOT/build/swanc" build "$TMP/cross" --target riscv64 --emit-asm -o "$TMP/riscv64"
test -f "$TMP/riscv64/01_helper.s"
test -f "$TMP/riscv64/02_main.s"
mkdir -p "$TMP/wasm-src"
cat > "$TMP/wasm-src/01_helper.swan" <<'SWAN'
export func wasm_helper() -> i64:
    return 42
SWAN
cat > "$TMP/wasm-src/02_main.swan" <<'SWAN'
func main() -> i64:
    return 0
SWAN
"$ROOT/build/swanc" build "$TMP/wasm-src" --target wasm32 --emit-asm -o "$TMP/wasm"
test -f "$TMP/wasm/01_helper.wasm"
test -f "$TMP/wasm/02_main.wasm"
strings "$TMP/wasm/01_helper.wasm" | grep -F wasm_helper >/dev/null
"$ROOT/build/swanc" build "$TMP/cross" --target-triple aarch64-apple-macosx14.0.0 --emit-llvm -o "$TMP/llvm"
test -f "$TMP/llvm/01_helper.ll"
test -f "$TMP/llvm/02_main.ll"

printf 'PASS: per-file source buffers, source-aware diagnostics, and per-source artifacts on x86-64, AArch64, WASM and LLVM\n'


# Ten Swan source files must remain ten independent assembly translation units.
mkdir -p "$TMP/ten-source"
for n in 0 1 2 3 4 5 6 7 8; do
    value=$((n + 1))
    cat > "$TMP/ten-source/source_$n.swan" <<SWAN
export C func source_value_$n() -> int:
    let value = $value
    return value
SWAN
done
cat > "$TMP/ten-source/main.swan" <<'SWAN'
func main() -> int:
    return source_value_0() + source_value_1() + source_value_2() + source_value_3() + source_value_4() + source_value_5() + source_value_6() + source_value_7() + source_value_8() - 45
SWAN
"$ROOT/build/swanc" build "$TMP/ten-source" -o "$TMP/ten-source-app"
"$TMP/ten-source-app"
"$ROOT/build/swanc" build "$TMP/ten-source" --emit-asm -o "$TMP/ten-assembly"
test "$(find "$TMP/ten-assembly" -maxdepth 1 -name '*.s' | wc -l)" -eq 10
for n in 0 1 2 3 4 5 6 7 8; do
    test -f "$TMP/ten-assembly/source_$n.s"
done
test -f "$TMP/ten-assembly/main.s"
for n in 0 1 2 3 4 5 6 7 8; do
    grep -q "\\.globl source_value_$n" "$TMP/ten-assembly/source_$n.s"
    grep -q "\\.extern source_value_$n" "$TMP/ten-assembly/main.s"
done
printf 'PASS: ten Swan files produce ten independent assembly translation units and linked symbols\n'
mkdir -p "$TMP/ten-target"
for n in 0 1 2 3 4 5 6 7 8; do
    cat > "$TMP/ten-target/source_$n.swan" <<SWAN
export func target_value_$n() -> bool:
    return true
SWAN
done
cat > "$TMP/ten-target/main.swan" <<'SWAN'
func main() -> bool:
    return true
SWAN
"$ROOT/build/swanc" build "$TMP/ten-target" --target aarch64 --emit-asm -o "$TMP/ten-aarch64"
test "$(find "$TMP/ten-aarch64" -maxdepth 1 -name '*.s' | wc -l)" -eq 10
printf 'PASS: AArch64 emission also preserves one output artifact per Swan source\n'

