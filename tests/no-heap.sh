#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/no-heap"
mkdir -p "$TMP"

cat > "$TMP/stack.swan" <<'SWAN'
func main() -> int:
    let values = array_u32(16)
    values[0] = 21
    values[1] = 21
    return values[0] + values[1] - 42
SWAN
"$SWAN" build "$TMP/stack.swan" --no-heap -o "$TMP/stack"
"$TMP/stack"
"$SWAN" build "$TMP/stack.swan" --no-heap --emit-asm -o "$TMP/assembly"
if grep -q 'calloc@PLT' "$TMP/assembly/stack.s"; then
    echo "no-heap output unexpectedly calls calloc" >&2
    exit 1
fi

cat > "$TMP/dynamic.swan" <<'SWAN'
func main() -> int:
    let count = 16
    let values = array_u32(count)
    return 0
SWAN
if "$SWAN" build "$TMP/dynamic.swan" --no-heap -o "$TMP/dynamic" 2>"$TMP/dynamic-error"; then
    echo "expected dynamic array allocation to fail in no-heap mode" >&2
    exit 1
fi
grep -F 'no-heap mode' "$TMP/dynamic-error" >/dev/null

cat > "$TMP/escaped.swan" <<'SWAN'
func consume(values: u32_array) -> int:
    return values[0]
func main() -> int:
    let values = array_u32(16)
    return consume(values)
SWAN
if "$SWAN" build "$TMP/escaped.swan" --no-heap -o "$TMP/escaped" 2>"$TMP/escaped-error"; then
    echo "expected escaped array to fail in no-heap mode" >&2
    exit 1
fi
grep -F 'no-heap mode' "$TMP/escaped-error" >/dev/null

printf 'PASS: no-heap mode stack-promotes bounded arrays and rejects dynamic or escaping allocations\n'
