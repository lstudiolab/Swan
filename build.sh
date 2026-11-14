#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CC=${CC:-cc}
mkdir -p "$ROOT/build"

# Bootstrap the C11 host without linking the not-yet-built Swan SIL object.
"$CC" -std=c11 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration -Werror=return-type -Werror=incompatible-pointer-types -DSWAN_SIL_BOOTSTRAP \
    -I"$ROOT/compiler/include" "$ROOT/compiler/tools/swanc/main.c" -o "$ROOT/build/swan-bootstrap"

# Compile the SIL passes from Swan source as one native object.
{
    for stage in silgen silverify sildemand silpass siloptimization; do
        sed '/^[[:space:]]*import[[:space:]]/d' "$ROOT/compiler/lib/SIL/Runtime/$stage.swan"
        printf '\n'
    done
} > "$ROOT/build/sil_runtime.swan"

# Keep every pass reachable from the temporary main, then rename that entry
# point so the generated object does not collide with the C host's main.
cat >> "$ROOT/build/sil_runtime.swan" <<'SWAN'
func main() -> int:
    let state = array_unchecked(1280)
    let workspace = array_unchecked(1024)
    var count = sil_emit_param(state, 0, 1 * 1048576 + 2 * 65536)
    count = sil_emit_const(state, count, 2 * 1048576 + 2 * 65536 + 20)
    count = sil_emit_binary(state, count, 34 * 16777216 + 3 * 65536 + 1 * 256 + 2)
    count = sil_emit_return(state, count, 3)
    return sil_optimize(state, count, workspace)
SWAN
"$ROOT/build/swan-bootstrap" build "$ROOT/build/sil_runtime.swan" \
    --emit-asm -o "$ROOT/build/sil_runtime.s"
sed -i -E 's/(^|[^[:alnum:]_])main([^[:alnum:]_]|$)/\1swan_sil_build_stub\2/g' "$ROOT/build/sil_runtime.s"
"$CC" -c "$ROOT/build/sil_runtime.s" -o "$ROOT/build/sil_runtime.o"

# Link the Swan-generated SIL implementation into the production compiler.
"$CC" -std=c11 -O2 -DNDEBUG -Wall -Wextra -Wpedantic -Werror=implicit-function-declaration -Werror=return-type -Werror=incompatible-pointer-types \
    -I"$ROOT/compiler/include" "$ROOT/compiler/tools/swanc/main.c" "$ROOT/build/sil_runtime.o" \
    -o "$ROOT/build/swanc"
ln -sf swanc "$ROOT/build/swan"
printf 'Built swanc compiler: %s\n' "$ROOT/build/swanc"
printf 'Frontend and orchestration: C11 host\n'
printf 'SIL generation, verification, demand and optimization: Swan source\n'
printf 'Typed IR, IR optimization and target backends: Swan-owned\n'
