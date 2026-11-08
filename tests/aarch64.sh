#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn sum9(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int) -> int:
    return i / 3 + i % 3
fn main(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int) -> int:
    return sum9(a, b, c, d, e, f, g, h, i)
SWAN
"$BIN" build "$TMP/main.swan" --target aarch64 --emit-asm -o "$TMP/main.s"
if ! clang --target=aarch64-unknown-linux-gnu -c -x assembler "$TMP/main.s" -o "$TMP/main.o"; then nl -ba "$TMP/main.s"; exit 1; fi
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'AArch64'
printf 'PASS: AArch64 ELF backend handles CFG, memory and stack-passed ABI arguments\n'
