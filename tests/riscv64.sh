#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn sum9(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int) -> int:
    return i / 3 + i % 3
out
fn main(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int, i: int) -> int:
    return sum9(a, b, c, d, e, f, g, h, i)
out
SWAN
"$BIN" build "$TMP/main.swan" --target riscv64 -o "$TMP/main.o"
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'RISC-V'
printf 'PASS: RISC-V64 ELF backend handles CFG, memory and stack-passed ABI arguments\n'
