#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
sh build.sh
"$ROOT/build/swan" run tests/sil_pipeline.swan --stdlib "$ROOT/compiler/sil"
printf 'Native Swan-authored SIL generation, verification, folding, liveness and DCE passed.\n'
