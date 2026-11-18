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

cat > "$TMP/i32.swan" <<'SWAN'
func combine(a: i32, b: i32) -> i32:
    var total: i32 = a + b
    total = total * 3
    total = total - 2
    let quotient: i32 = total / 3
    let remainder: i32 = total % 3
    return quotient + remainder
func main() -> i32:
    return combine(-7, 2)
SWAN
result=$("$BIN" run "$TMP/i32.swan" --target wasm64 2>"$TMP/i32-source-runtime.err")
test "$result" = "-7"
"$BIN" build "$TMP/i32.swan" --target wasm64 -o "$TMP/i32.wasm"
result=$("$BIN" run "$TMP/i32.wasm" 2>"$TMP/i32-runtime.err")
test "$result" = "-7"
printf 'PASS: WASM64 i32 arithmetic, signed division, and function ABI\n'
