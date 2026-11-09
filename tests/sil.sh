#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
sh build.sh

# Exercise the Swan-authored stage modules through the normal module loader.
"$ROOT/build/swan" run tests/sil_pipeline.swan --stdlib "$ROOT/compiler/sil"

# Exercise the compiler's direct C-host -> Swan SIL -> typed SSA bridge.
"${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" \
    "$ROOT/tests/sil_bridge.c" "$ROOT/build/sil_runtime.o" -o "$ROOT/build/sil_bridge_test"
"$ROOT/build/sil_bridge_test"

# Exercise the normal native compile/run path, which now prefers SIL.
cat > "$ROOT/build/sil_native_smoke.swan" <<'SWAN'
fn main() -> int:
    return 40 + 2
SWAN
"$ROOT/build/swan" build "$ROOT/build/sil_native_smoke.swan" -o "$ROOT/build/sil_native_smoke"
if "$ROOT/build/sil_native_smoke"; then
    status=0
else
    status=$?
fi
if [ "$status" -ne 42 ]; then
    printf 'SIL native smoke test returned %s; expected 42.\n' "$status" >&2
    exit 1
fi
printf 'Swan-authored SIL pipeline, bridge, and native path passed.\n'
