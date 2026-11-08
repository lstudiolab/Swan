#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn main(count: i32, step: i32) -> i32:
    var value = count
    while value < step:
        value = value + step
    return value
SWAN
"$BIN" build "$TMP/main.swan" --target armv7 -o "$TMP/main.o"
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'ARM'
printf 'PASS: ARMv7 backend emits ELF for i32 loops and mutable locals\n'
