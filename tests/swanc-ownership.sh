#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
SOURCE="$ROOT/build/swanc-ownership-module.swan"
ASSEMBLY="$ROOT/build/swanc-ownership-module.s"
{
    cat "$ROOT/swanc/sema/Ownership.swan"
    printf '\n'
} > "$SOURCE"
cat >> "$SOURCE" <<'SWAN'
fn main() -> int:
    return swanc_ownership_analyze(array_unchecked(4), 0, 0, array_unchecked(4), 4)
out
SWAN
"$ROOT/build/swan" "$SOURCE" --target x86_64 --emit-asm -o "$ASSEMBLY"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_ownership_module_stub\2/g' "$ASSEMBLY"
cc -O2 "$ASSEMBLY" "$ROOT/tests/swanc_ownership.c" -o "$ROOT/build/swanc-ownership-test"
"$ROOT/build/swanc-ownership-test"
