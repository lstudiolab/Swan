#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
[ -x build/brewc ] || sh build.sh
REPORT=$(mktemp)
OUTPUT=$(mktemp)
trap 'rm -f "$REPORT" "$OUTPUT"' EXIT
./build/brewc build --profile examples/hello.brew -o "$OUTPUT" 2>"$REPORT"
grep -q "SIL generation and optimization" "$REPORT"
grep -q "Compiler-wide optimization passes:" "$REPORT"
cat > "$ROOT/build/sil-control-flow.brew" <<'BREW'
func main() -> int:
    var value = 0
    while value < 4
        if value < 2
            value = value + 1
        else
            value = value + 1
    return value
BREW
./build/brewc build "$ROOT/build/sil-control-flow.brew" -o "$OUTPUT"
printf 'Brew SIL pipeline and nested-control-flow tests passed\n'
