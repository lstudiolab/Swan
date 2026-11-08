#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn main() -> int:
    var count = 0
    while count < 4:
        count = count + 1
    if count == 4:
        return count * 10
    else:
        return 1
SWAN
"$BIN" build "$TMP/main.swan" --target riscv64 -o "$TMP/main.o"
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'RISC-V'
printf 'PASS: RISC-V64 backend emits ELF for loops, branches and mutable locals\n'
