#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn double(x: int) -> int:
    return x * 2
fn main() -> int:
    var count = 0
    var total = 0
    while count < 8:
        count = count + 1
        if count == 3:
            continue
        total = total + double(count)
    if false and explode():
        return 1
    return total
fn explode() -> bool:
    return 1 / 0 == 0
SWAN
"$BIN" build "$TMP/main.swan" -o "$TMP/main"
set +e
"$TMP/main"
STATUS=$?
set -e
test "$STATUS" -eq 66
printf 'PASS: x86-64 production path lowers verified IR with calls, loops, memory and short-circuit logic\n'
