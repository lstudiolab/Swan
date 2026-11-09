#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/profile-test"
mkdir -p "$TMP"
output=$("$ROOT/build/swan" --profile "$ROOT/examples/hello.swan" 2>"$TMP/profile.txt")
test "$output" = "Hello from Swan"
grep -q 'Swan compiler pipeline profile (seconds)' "$TMP/profile.txt"
for stage in 'Source file read' 'Lexing' 'Parsing' 'Semantic analysis' 'IR generation' 'IR optimization' 'Target code generation' 'Native linking' 'Program execution' 'Total wall time'; do
    grep -q "$stage" "$TMP/profile.txt"
done
awk '/ s/ { for (i=1;i<=NF;i++) if ($i=="s") {v=$(i-1); if (v !~ /^[0-9]+([.][0-9]+)?$/) exit 1} }' "$TMP/profile.txt"
printf 'PASS: compiler pipeline profile reports per-stage seconds\n'
