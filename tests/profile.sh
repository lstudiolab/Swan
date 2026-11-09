#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/profile-test"
mkdir -p "$TMP"
"$ROOT/build/swan" build --profile "$ROOT/benchmarks/compiler_pipeline.swan" -o "$TMP/compiler-pipeline" 2>"$TMP/profile.txt"
grep -q 'Swan compiler pipeline profile (seconds)' "$TMP/profile.txt"
for stage in 'Source file read' 'Lexing' 'Parsing' 'Semantic analysis' 'Reachability analysis' 'IR generation' 'IR optimization' 'Target code generation' 'Assembly output' 'Native linking' 'Total wall time'; do
    grep -q "$stage" "$TMP/profile.txt"
done
if grep -q 'AST fallback code generation' "$TMP/profile.txt"; then
    echo 'FAIL: profiling workload unexpectedly used AST fallback instead of typed IR' >&2
    exit 1
fi
awk '/ s/ { for (i=1;i<=NF;i++) if ($i=="s") {v=$(i-1); if (v !~ /^[0-9]+([.][0-9]+)?$/) exit 1} }' "$TMP/profile.txt"
cat "$TMP/profile.txt"
printf 'PASS: typed-IR compiler pipeline benchmark reports seconds per stage\n'
