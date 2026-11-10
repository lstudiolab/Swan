#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
CLANG=${CLANG:-clang}
TRIPLE=$("$CLANG" -dumpmachine)
cat > "$TMP/main.swan" <<'SWAN'
func twice(value: int) -> int:
    return value * 2
out
func main() -> int:
    return twice(21)
out
SWAN
"$BIN" build "$TMP/main.swan" --target-triple "$TRIPLE" -o "$TMP/main.o"
test -s "$TMP/main.o"
"$BIN" build "$TMP/main.swan" --target-triple "$TRIPLE" --emit-llvm -o "$TMP/main.ll"
grep -q "target triple = \"$TRIPLE\"" "$TMP/main.ll"
grep -q "define i64 @main" "$TMP/main.ll"
"$BIN" build "$TMP/main.swan" --target-triple "$TRIPLE" --emit-asm -o "$TMP/main.s"
test -s "$TMP/main.s"
grep -q "twice" "$TMP/main.s"
if "$CLANG" -print-targets | grep -qi 'wasm32'; then
    "$BIN" build "$TMP/main.swan" --target-triple wasm32-unknown-unknown -o "$TMP/main-wasm.o"
    test -s "$TMP/main-wasm.o"
    python3 - "$TMP/main-wasm.o" <<'PY'
import sys
data = open(sys.argv[1], "rb").read()
assert data[:4] == b"\0asm", "expected a WebAssembly relocatable object"
PY
fi
printf 'PASS: arbitrary LLVM target triple, LLVM IR, target assembly and object emission\n'
