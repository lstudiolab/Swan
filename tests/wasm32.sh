#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
func explode() -> bool:
    return 1 / 0 == 0

func double(x: int) -> int:
    return x * 2

func recurse(n: int) -> int:
    var value = n
    if n <= 0:
        return value
    out
    else:
        return value + recurse(n - 1)


func main() -> int:
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
    if true or explode():
        return total + recurse(4)
    out
    return 0

SWAN
"$BIN" build "$TMP/main.swan" --target wasm32 -o "$TMP/main.wasm"
test -s "$TMP/main.wasm"
node -e 'const fs=require("fs"); WebAssembly.instantiate(fs.readFileSync(process.argv[1])).then(({instance})=>{if(instance.exports.main()!==76n)process.exit(1);}).catch(e=>{console.error(e);process.exit(1);});' "$TMP/main.wasm"
printf 'PASS: WASM32 emits and executes loops, branches, mutable locals, recursion, calls and short-circuit PHIs\n'
