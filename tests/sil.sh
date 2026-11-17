#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
[ -x build/swanc ] || sh build.sh
REPORT=$(mktemp)
OUTPUT=$(mktemp)
trap 'rm -f "$REPORT" "$OUTPUT"' EXIT
./build/swanc build --profile examples/hello.swan -o "$OUTPUT" 2>"$REPORT"
grep -q "SIL generation and optimization" "$REPORT"
grep -q "Compiler-wide optimization passes:" "$REPORT"
cat > "$ROOT/build/sil-control-flow.swan" <<'SWAN'
func main() -> int:
    var value = 0
    while value < 4
        if value < 2
            value = value + 1
        else
            value = value + 1
    return value
SWAN
./build/swanc build "$ROOT/build/sil-control-flow.swan" -o "$OUTPUT"
printf 'Swan SIL pipeline and nested-control-flow tests passed\n'
