#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
SOURCE="$ROOT/build/swanc-optimizer-module.swan"
ASSEMBLY="$ROOT/build/swanc-optimizer-module.s"
{
    cat "$ROOT/swanc/ir/IR.swan"
    printf '\n'
    cat "$ROOT/swanc/optimization/ConstantFold.swan"
    printf '\n'
    cat "$ROOT/swanc/optimization/UseCountAnalysis.swan"
    printf '\n'
    cat "$ROOT/swanc/optimization/DeadCodeElimination.swan"
    printf '\n'
    cat "$ROOT/swanc/optimization/Optimization.swan"
} > "$SOURCE"
cat >> "$SOURCE" <<'SWAN'
fn main() -> int:
    return swanc_ir_optimize(array_unchecked(8), 0, array_unchecked(4), 1)
out
SWAN
"$ROOT/build/swan" "$SOURCE" --target x86_64 --emit-asm -o "$ASSEMBLY"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_optimizer_module_stub\2/g' "$ASSEMBLY"
cc -O2 "$ASSEMBLY" "$ROOT/tests/swanc_optimizer.c" -o "$ROOT/build/swanc-optimizer-test"
"$ROOT/build/swanc-optimizer-test"
