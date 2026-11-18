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
# An explicitly emitted portable module is retained after native JIT execution
# so the same artifact can still be deployed to a browser.
test -s "$TMP/main.wasm"
test ! -e "$TMP/main.wasm.cwasm"
"$BIN" compile "$TMP/main.swan" -o "$TMP/portable.wasm"
test -s "$TMP/portable.wasm"
result=$("$BIN" run "$TMP/portable.wasm" 2>"$TMP/portable-runtime.err")
test "$result" = "3"
test -s "$TMP/portable.wasm"
printf 'PASS: swanc compile emits portable WASM64 and SIC runs it through the host JIT\n'
