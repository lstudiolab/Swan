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
    cat "$ROOT/swanc/optimization/Optimization.swan"
    printf '\nextern fn swan_optimizer_test_main() -> int\\nfn main() -> int:\\n    return swan_optimizer_test_main()\\nout\\n'
} > "$SOURCE"
cat >> "$SOURCE" <<'SWAN'
fn main() -> int:
    return 0
out
SWAN
"$ROOT/build/swan" "$SOURCE" --target x86_64 --emit-asm -o "$ASSEMBLY"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\\1swan_optimizer_module_stub\\2/g' "$ASSEMBLY"
cc -O2 "$ASSEMBLY" "$ROOT/tests/swanc_optimizer.c" -o "$ROOT/build/swanc-optimizer-test"
"$ROOT/build/swanc-optimizer-test"
