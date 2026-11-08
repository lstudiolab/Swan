#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
printf 'fn main() -> int:\n    return 42\n' > "$TMP/main.swan"
"$BIN" build "$TMP/main.swan" --target aarch64 -o "$TMP/main.o"
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'AArch64'
printf 'PASS: Swan emits an AArch64 ELF object\n'
