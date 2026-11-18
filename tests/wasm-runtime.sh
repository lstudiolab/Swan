#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swanc"
if ! command -v wasmtime >/dev/null 2>&1; then
    echo "SKIP: Wasmtime is not installed"
    exit 0
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
func main() -> int:
    var count = 0
    while count < 3:
        count = count + 1
    return count
SWAN
result=$("$BIN" run "$TMP/main.swan" --target wasm64 2>"$TMP/source-runtime.err")
test "$result" = "3"
"$BIN" build "$TMP/main.swan" --target wasm64 -o "$TMP/main.wasm"
result=$("$BIN" run "$TMP/main.wasm" 2>"$TMP/runtime.err")
test "$result" = "3"
# Native SIC compilation produces a host-specific Wasmtime artifact and keeps
# an explicitly supplied portable WASM file available for browser deployment.
test -s "$TMP/main.wasm.cwasm"
test -s "$TMP/main.wasm"
printf 'PASS: Swan runs a memory64 module through Wasmtime and invokes main\n'
