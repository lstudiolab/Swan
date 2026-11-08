#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
printf 'fn twice(x: int) -> int:\n    return x * 2\nfn main() -> int:\n    return twice(21)\n' > "$TMP/main.swan"
"$BIN" build "$TMP/main.swan" --target wasm32 -o "$TMP/main.wasm"
test -s "$TMP/main.wasm"
MAGIC=$(od -An -tx1 -N8 "$TMP/main.wasm" | tr -d ' \n')
test "$MAGIC" = "0061736d01000000"
node -e 'const fs=require("fs"); WebAssembly.instantiate(fs.readFileSync(process.argv[1])).then(({instance})=>{if(instance.exports.main()!==42n)process.exit(1);}).catch(e=>{console.error(e);process.exit(1);});' "$TMP/main.wasm"
printf 'PASS: Swan emits and executes a WebAssembly 1.0 module with a function call\n'
