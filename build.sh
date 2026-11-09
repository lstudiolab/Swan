#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
mkdir -p "$ROOT/build"

# Bootstrap the C11 host without linking the not-yet-built Swan SIL object.
"$CC" -std=c11 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -DSWAN_SIL_BOOTSTRAP \
    -I"$ROOT/compiler/include" "$ROOT/main.c" -o "$ROOT/build/swan-bootstrap"

# The compiler's SIL passes are source files in Swan. Concatenate their
# function definitions so the bootstrap compiler can emit one linkable object.
{
    for stage in silgen silverify sildemand silpass siloptimization; do
        sed '/^[[:space:]]*import[[:space:]]/d' "$ROOT/compiler/sil/$stage.swan"
        printf '\n'
    done
} > "$ROOT/build/sil_runtime.swan"
cat >> "$ROOT/build/sil_runtime.swan" <<'SWAN'
fn main() -> int:
    return 0
SWAN

"$ROOT/build/swan-bootstrap" build "$ROOT/build/sil_runtime.swan" \
    --emit-asm -o "$ROOT/build/sil_runtime.s"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_sil_build_stub\2/g' "$ROOT/build/sil_runtime.s"
"$CC" -c "$ROOT/build/sil_runtime.s" -o "$ROOT/build/sil_runtime.o"

# The final compiler executes Swan-generated SIL before typed IR generation.
"$CC" -std=c11 -O2 -DNDEBUG -Wall -Wextra -Wpedantic \
    -I"$ROOT/compiler/include" "$ROOT/main.c" "$ROOT/build/sil_runtime.o" \
    -o "$ROOT/build/swan"

printf 'Built Swan compiler: %s\n' "$ROOT/build/swan"
printf 'Frontend and orchestration: C11 host\n'
printf 'SIL generation, verification, demand and optimization: Swan source\n'
printf 'Typed IR, IR optimization and target backends: Swan-owned\n'
