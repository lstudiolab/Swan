#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
import foundation
fn explode() -> bool:
    return 1 / 0 == 0
fn double(x: int) -> int:
    return x * 2
fn recurse(n: int) -> int:
    var value = n
    if n <= 0:
        return value
    else:
        return value + recurse(n - 1)
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
    if true or explode():
        return total + recurse(4) + square_int(3)
    return 0
SWAN
"$BIN" build "$TMP/main.swan" --target wasm32 -o "$TMP/main.wasm"
test -s "$TMP/main.wasm"
node -e 'const fs=require("fs"); WebAssembly.instantiate(fs.readFileSync(process.argv[1])).then(({instance})=>{if(instance.exports.main()!==85n)process.exit(1);}).catch(e=>{console.error(e);process.exit(1);});' "$TMP/main.wasm"
printf 'PASS: WASM32 emits and executes loops, branches, mutable locals, recursion, calls and short-circuit PHIs\n'
