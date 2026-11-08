#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
printf 'fn twice(x: int) -> int:\n    return x * 2\nfn main() -> int:\n    return twice(21)\n' > "$TMP/main.swan"
"$BIN" build "$TMP/main.swan" --target riscv64 -o "$TMP/main.o"
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'RISC-V'
printf 'PASS: Swan emits a RISC-V64 ELF object\n'
