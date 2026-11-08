#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn main() -> i32:
    var count: i32 = 0
    while count < 4:
        count = count + 1
    if count == 4:
        return count * 10
    else:
        return 1
SWAN
"$BIN" build "$TMP/main.swan" --target armv7 -o "$TMP/main.o"
test -s "$TMP/main.o"
readelf -h "$TMP/main.o" | grep -q 'ARM'
printf 'PASS: ARMv7 backend emits ELF for loops, branches and mutable locals\n'
