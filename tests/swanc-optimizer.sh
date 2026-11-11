#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
SOURCE="$ROOT/build/swanc-optimizer-test.swan"
{
    cat "$ROOT/swanc/ir/IR.swan"
    printf '\n'
    cat "$ROOT/swanc/optimization/ConstantFold.swan"
    printf '\n'
    cat "$ROOT/swanc/optimization/Optimization.swan"
    printf '\n'
    cat "$ROOT/tests/swanc_optimizer.swan"
} > "$SOURCE"
"$ROOT/build/swan" build "$SOURCE" -o "$ROOT/build/swanc-optimizer-test"
"$ROOT/build/swanc-optimizer-test"
printf 'Compiler 2 linear-IR optimizer test passed.\n'
