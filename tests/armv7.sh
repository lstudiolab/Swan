#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
printf 'fn main(x: i32) -> i32:\\n    return x\\n' > "$TMP/main.swan"
"$BIN" build "$TMP/main.swan" --target armv7 -o "$TMP/main.o"
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'ARM'
printf 'PASS: Swan emits an ARMv7 ELF object for i32 scalar code\n'
