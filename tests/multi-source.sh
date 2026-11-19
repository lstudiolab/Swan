#!/usr/bin/env sh
set -eu
set -x
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/multi-source"
rm -rf "$TMP"
mkdir -p "$TMP/project"

cat > "$TMP/project/01_math.brew" <<'BREW'
export C func brew_answer() -> int:
    return 42
BREW

cat > "$TMP/project/02_main.brew" <<BREW
import C "$TMP/project/c_api.h"
func main() -> int:
    return brew_answer() + c_answer(42) - 126
BREW

cat > "$TMP/project/c_api.h" <<'C'
#include <stdint.h>
int64_t c_answer(int64_t value);
C
cat > "$TMP/project/native.c" <<'C'
#include <stdint.h>
int64_t brew_answer(void);
int64_t c_answer(int64_t value) { return value + brew_answer(); }
C

"$ROOT/build/brewc" build "$TMP/project" -o "$TMP/project-app"
"$TMP/project-app"

"$ROOT/build/brewc" build "$TMP/project" --emit-asm -o "$TMP/assembly"
test -f "$TMP/assembly/01_math.s"
test -f "$TMP/assembly/02_main.s"
test -f "$TMP/assembly/native_0.s"
grep -q 'brew_answer' "$TMP/assembly/01_math.s"
grep -q 'c_answer' "$TMP/assembly/02_main.s"
grep -q 'main' "$TMP/assembly/02_main.s"
grep -q '\.extern brew_answer' "$TMP/assembly/02_main.s"
grep -q '\.extern c_answer' "$TMP/assembly/02_main.s"
grep -q 'c_answer' "$TMP/assembly/native_0.s"

cat > "$TMP/bad.brew" <<'BREW'
func main() -> int:
    return (
BREW
if "$ROOT/build/brewc" build "$TMP/bad.brew" -o "$TMP/invalid" 2>"$TMP/diagnostic"; then
    echo "expected malformed source to fail" >&2
    exit 1
fi
grep -F "$TMP/bad.brew:" "$TMP/diagnostic" >/dev/null

cat > "$TMP/bad-name.brew" <<'BREW'
func main() -> int:
    return missing_value
BREW
if "$ROOT/build/brewc" build "$TMP/bad-name.brew" -o "$TMP/invalid-name" 2>"$TMP/name-diagnostic"; then
    echo "expected unresolved name to fail" >&2
    exit 1
fi
grep -F "$TMP/bad-name.brew:" "$TMP/name-diagnostic" >/dev/null

cat > "$TMP/warning.brew" <<'BREW'
func unused_helper() -> i64:
    return 7
func main() -> int:
    return 0
BREW
"$ROOT/build/brewc" build "$TMP/warning.brew" -o "$TMP/warning" 2>"$TMP/warning-diagnostic"
grep -F "$TMP/warning.brew:" "$TMP/warning-diagnostic" >/dev/null

mkdir -p "$TMP/cross"
cat > "$TMP/cross/01_helper.brew" <<'BREW'
export func cross_value() -> bool:
    return true
BREW
cat > "$TMP/cross/02_main.brew" <<'BREW'
func main() -> bool:
    return true
BREW
"$ROOT/build/brewc" build "$TMP/cross" --target aarch64 --emit-asm -o "$TMP/aarch64"
test -f "$TMP/aarch64/01_helper.s"
test -f "$TMP/aarch64/02_main.s"
mkdir -p "$TMP/wasm64-src"
cat > "$TMP/wasm64-src/01_helper.brew" <<'BREW'
export func wasm_helper() -> i64:
    return 42
BREW
cat > "$TMP/wasm64-src/02_main.brew" <<'BREW'
func main() -> i64:
    return 0
BREW
"$ROOT/build/brewc" build "$TMP/wasm64-src" --target wasm64 --emit-asm -o "$TMP/wasm64"
test -f "$TMP/wasm64/01_helper.wasm"
test -f "$TMP/wasm64/02_main.wasm"
strings "$TMP/wasm64/01_helper.wasm" | grep -F wasm_helper >/dev/null

printf 'PASS: per-file source buffers, source-aware diagnostics, and per-source artifacts on x86-64, AArch64, and WASM64\n'


# Ten Brew source files must remain ten independent assembly translation units.
mkdir -p "$TMP/ten-source"
for n in 0 1 2 3 4 5 6 7 8; do
    value=$((n + 1))
    cat > "$TMP/ten-source/source_$n.brew" <<BREW
export C func source_value_$n() -> int:
    let value = $value
    return value
BREW
done
cat > "$TMP/ten-source/main.brew" <<'BREW'
func main() -> int:
    return source_value_0() + source_value_1() + source_value_2() + source_value_3() + source_value_4() + source_value_5() + source_value_6() + source_value_7() + source_value_8() - 45
BREW
"$ROOT/build/brewc" build "$TMP/ten-source" -o "$TMP/ten-source-app"
"$TMP/ten-source-app"
"$ROOT/build/brewc" build "$TMP/ten-source" --emit-asm -o "$TMP/ten-assembly"
test "$(find "$TMP/ten-assembly" -maxdepth 1 -name '*.s' | wc -l)" -eq 10
for n in 0 1 2 3 4 5 6 7 8; do
    test -f "$TMP/ten-assembly/source_$n.s"
done
test -f "$TMP/ten-assembly/main.s"
for n in 0 1 2 3 4 5 6 7 8; do
    grep -q "\\.globl source_value_$n" "$TMP/ten-assembly/source_$n.s"
    grep -q "\\.extern source_value_$n" "$TMP/ten-assembly/main.s"
done
printf 'PASS: ten Brew files produce ten independent assembly translation units and linked symbols\n'
mkdir -p "$TMP/ten-target"
for n in 0 1 2 3 4 5 6 7 8; do
    cat > "$TMP/ten-target/source_$n.brew" <<BREW
export func target_value_$n() -> bool:
    return true
BREW
done
cat > "$TMP/ten-target/main.brew" <<'BREW'
func main() -> bool:
    return true
BREW
"$ROOT/build/brewc" build "$TMP/ten-target" --target aarch64 --emit-asm -o "$TMP/ten-aarch64"
test "$(find "$TMP/ten-aarch64" -maxdepth 1 -name '*.s' | wc -l)" -eq 10
printf 'PASS: AArch64 emission also preserves one output artifact per Brew source\n'

