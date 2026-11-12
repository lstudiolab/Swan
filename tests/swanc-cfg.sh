#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
SOURCE="$ROOT/build/swanc-cfg-module.swan"
ASSEMBLY="$ROOT/build/swanc-cfg-module.s"
{
    cat "$ROOT/swanc/sema/CFG.swan"
    printf '\n'
} > "$SOURCE"
cat >> "$SOURCE" <<'SWAN'
fn main() -> int:
    let blocks = array_unchecked(5)
    let workspace = array_unchecked(19)
    workspace[0] = 0
    return swanc_cfg_verify(blocks, 1, workspace, 19)
out
SWAN
"$ROOT/build/swan" "$SOURCE" --target x86_64 --emit-asm -o "$ASSEMBLY"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_cfg_module_stub\2/g' "$ASSEMBLY"
cc -O2 "$ASSEMBLY" "$ROOT/tests/swanc_cfg.c" -o "$ROOT/build/swanc-cfg-test"
"$ROOT/build/swanc-cfg-test"
