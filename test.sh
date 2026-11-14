#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$ROOT"
sh "$ROOT/build.sh"
sh "$ROOT/tests/headers.sh"
sh "$ROOT/tests/ir-verifier.sh"
sh "$ROOT/tests/ir-codegen-phi.sh"
sh "$ROOT/tests/sil-bridge.sh"
sh "$ROOT/tests/diagnostics.sh"
sh "$ROOT/tests/c11-smoke.sh"
sh "$ROOT/tests/ffi.sh"
sh "$ROOT/tests/multi-source.sh"
sh "$ROOT/tests/utf8.sh"
sh "$ROOT/tests/profile.sh"
sh "$ROOT/tests/smoke.sh"
sh "$ROOT/tests/regressions.sh"
sh "$ROOT/tests/foundation.sh"
sh "$ROOT/tests/lazy-imports.sh"
sh "$ROOT/tests/architecture.sh"

cc -std=c11 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" "$ROOT/tests/keywords.c" -o "$ROOT/build/keywords-test"
"$ROOT/build/keywords-test"
