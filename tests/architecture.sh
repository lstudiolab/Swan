#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/targets"
mkdir -p "$TMP"

cat > "$TMP/targets.swan" <<'SWAN'
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

# Every target is lowered from the same Swan AST to LLVM IR and then to target assembly.
for target in x86_64 aarch64 armv7 riscv64 wasm32; do
    "$SWAN" "$TMP/targets.swan" --target "$target" --emit-asm -o "$TMP/$target.s"
    test -s "$TMP/$target.s"
done

# LLVM's assembler validates the emitted ISA syntax and object format.
for target in x86_64 aarch64 armv7 riscv64 wasm32; do
    case "$target" in
        x86_64) triple=x86_64-unknown-linux-gnu ;;
        aarch64) triple=aarch64-unknown-linux-gnu ;;
        armv7) triple=armv7-unknown-linux-gnueabihf ;;
        riscv64) triple=riscv64-unknown-linux-gnu ;;
        wasm32) triple=wasm32-unknown-unknown ;;
    esac
    clang -target "$triple" -c -x assembler "$TMP/$target.s" -o "$TMP/$target.o"
done

# Platform triples select their own ABI and object format.
for target in arm64-apple-ios aarch64-apple-darwin x86_64-pc-windows-msvc aarch64-linux-android24 armv7a-linux-androideabi24; do
    safe=$(printf '%s' "$target" | tr '-' '_')
    "$SWAN" "$TMP/targets.swan" --target "$target" --emit-asm -o "$TMP/$safe.s"
    test -s "$TMP/$safe.s"
done

test "$("$SWAN" "$TMP/targets.swan")" = "91"
printf 'PASS: native LLVM IR lowering and execution\n'
printf 'PASS: x86-64, AArch64, ARMv7, RISC-V 64, and WebAssembly assembly\n'
printf 'PASS: Apple, Android, and Windows target triples\n'
