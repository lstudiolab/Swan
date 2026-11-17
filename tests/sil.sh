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
grep -q "SIL" "$REPORT"
printf 'Swan SIL pipeline smoke test passed\n'
