#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/profile-test"
mkdir -p "$TMP"
"$ROOT/build/brewc" build --profile "$ROOT/benchmarks/compiler_pipeline.brew" -o "$TMP/compiler-pipeline" 2>"$TMP/profile.txt"
grep -q 'Brew compiler pipeline profile (seconds)' "$TMP/profile.txt"
for stage in 'Source file read' 'Lexing' 'Parsing' 'Semantic analysis' 'Reachability analysis' 'IR generation' 'IR verification' 'IR optimization' 'IR pass: constant folding' 'IR pass: CFG simplification' 'IR pass: algebraic simplification' 'IR pass: copy propagation' 'IR pass: common-subexpression elimination' 'IR pass: loop-invariant code motion' 'IR pass: dead-code elimination' 'Target code generation' 'Assembly output' 'Native linking' 'Total wall time' 'IR pass executions:'; do
    grep -q "$stage" "$TMP/profile.txt"
done
if grep -q 'AST fallback code generation' "$TMP/profile.txt"; then
    echo 'FAIL: profiling workload unexpectedly used AST fallback instead of typed IR' >&2
    exit 1
fi
awk '/ s/ { for (i=1;i<=NF;i++) if ($i=="s") {v=$(i-1); if (v !~ /^[0-9]+([.][0-9]+)?$/) exit 1} }' "$TMP/profile.txt"
cat "$TMP/profile.txt"
printf 'PASS: typed-IR compiler pipeline benchmark reports seconds per stage\n'
