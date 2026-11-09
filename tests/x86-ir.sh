#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn sum8(a: int, b: int, c: int, d: int, e: int, f: int, g: int, h: int) -> int:
    return h
out
fn explode() -> bool:
    return 1 / 0 == 0
out
fn double(x: int) -> int:
    return x * 2
out
fn main() -> int:
    var count = 0
    var total = 0
    while count < 8:
        count = count + 1
        if count == 3:
            continue
        out
        total = total + double(count)
    out
    if false and explode():
        return 1
    out
    return total + sum8(1, 2, 3, 4, 5, 6, 7, 8)
out
SWAN
"$BIN" build "$TMP/main.swan" -o "$TMP/main"
set +e
"$TMP/main"
STATUS=$?
set -e
test "$STATUS" -eq 74
printf 'PASS: x86-64 IR backend handles loops, memory, short-circuit logic and stack-passed call arguments\n'
