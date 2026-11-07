#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
INCLUDE="$ROOT/compiler/include/swan"

cpp_files=$(find "$ROOT" -type f -name '*.cpp' ! -path "$ROOT/build/*" -print | sort)
if [ "$cpp_files" != "$ROOT/main.cpp" ]; then
    echo "FAIL: expected main.cpp to be the only C++ translation unit" >&2
    printf '%s\n' "$cpp_files" >&2
    exit 1
fi

reject_dependency() {
    subsystem=$1
    forbidden=$2
    directory="$INCLUDE/$subsystem"

    if grep -R -n -E "^[[:space:]]*#include[[:space:]]*[<\"]swan/($forbidden)/" "$directory"; then
        echo "FAIL: $subsystem depends on a higher-level subsystem" >&2
        exit 1
    fi
}

reject_dependency Basic 'Support|Lex|AST|Parse|Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency Support 'Lex|AST|Parse|Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency Lex 'AST|Parse|Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency AST 'Lex|Parse|Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency Parse 'Sema|Frontend|CodeGen|Compiler|Driver'
reject_dependency Sema 'Frontend|CodeGen|Compiler|Driver'
reject_dependency Frontend 'CodeGen|Compiler|Driver'
reject_dependency CodeGen 'Frontend|Compiler|Driver'
reject_dependency Compiler 'Driver'

# The full Swan frontend can lower optimized code to independent target ISAs.
cat > "$ROOT/build/portable-targets.swan" <<'SWAN'
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
for target in x86_64 aarch64 armv7 riscv64 wasm32; do
    "$ROOT/build/swan" "$ROOT/build/portable-targets.swan" --target "$target" --emit-asm -o "$ROOT/build/portable-$target.s"
    test -s "$ROOT/build/portable-$target.s"
done
grep -q 'main:' "$ROOT/build/portable-x86_64.s"
grep -q 'main:' "$ROOT/build/portable-aarch64.s"
grep -q 'main:' "$ROOT/build/portable-armv7.s"
grep -q 'main:' "$ROOT/build/portable-riscv64.s"
grep -q 'main:' "$ROOT/build/portable-wasm32.s"
for target in arm64-apple-ios aarch64-apple-darwin x86_64-pc-windows-msvc aarch64-linux-android24 armv7a-linux-androideabi24; do
    safe=$(printf '%s' "$target" | tr '-' '_')
    "$ROOT/build/swan" "$ROOT/build/portable-targets.swan" --target "$target" --emit-asm -o "$ROOT/build/portable-$safe.s"
    test -s "$ROOT/build/portable-$safe.s"
done
for target in x86_64 aarch64 armv7 riscv64 wasm32; do
    case "$target" in
        x86_64) triple=x86_64-linux-gnu ;;
        aarch64) triple=aarch64-linux-gnu ;;
        armv7) triple=armv7-linux-gnueabihf ;;
        riscv64) triple=riscv64-linux-gnu ;;
        wasm32) triple=wasm32-unknown-unknown ;;
    esac
    clang -target "$triple" -c -x assembler "$ROOT/build/portable-$target.s" -o "$ROOT/build/portable-$target.o"
done
for target in arm64-apple-ios aarch64-apple-darwin x86_64-pc-windows-msvc aarch64-linux-android24 armv7a-linux-androideabi24; do
    safe=$(printf '%s' "$target" | tr '-' '_')
    clang -target "$target" -c -x assembler "$ROOT/build/portable-$safe.s" -o "$ROOT/build/portable-$safe.o"
done
printf 'PASS: full Swan source lowers to x86-64, AArch64, ARMv7, RISC-V 64, WebAssembly, Apple, Android, and Windows assembly\n'

# The optimized portable backend executes the same array and function workload.
test "$("$ROOT/build/swan" "$ROOT/build/portable-targets.swan" --optimize)" = "91"
"$ROOT/build/swan" "$ROOT/build/portable-targets.swan" --emit-c -o "$ROOT/build/portable-targets.c"
cc -O3 -march=native -fwrapv "$ROOT/build/portable-targets.c" -o "$ROOT/build/portable-targets-c"
test "$("$ROOT/build/portable-targets-c")" = "91"
grep -q 'for (;' "$ROOT/build/portable-targets.c"
cat > "$ROOT/build/portable-continue.swan" <<'SWAN'
fn main() -> int:
    var i = 0
    var sum = 0
    while i < 5
        i = i + 1
        if i == 3
            continue
        sum = sum + i
    log sum
    return 0
SWAN
test "$("$ROOT/build/swan" "$ROOT/build/portable-continue.swan" --optimize)" = "12"
printf 'PASS: optimized portable backend execution\n'

printf 'PASS: compiler architecture boundaries\n'
