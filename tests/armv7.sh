#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn sum5(a: i32, b: i32, c: i32, d: i32, e: i32) -> i32:
    return e
fn main(a: i32, b: i32, c: i32, d: i32, e: i32) -> i32:
    return sum5(a, b, c, d, e)
SWAN
"$BIN" build "$TMP/main.swan" --target armv7 -o "$TMP/main.o"
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'ARM'
printf 'PASS: ARMv7 ELF backend handles i32 stack-passed AAPCS arguments\n'
