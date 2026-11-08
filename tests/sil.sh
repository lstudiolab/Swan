#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
sh build.sh
"$ROOT/build/swan" run tests/sil_pipeline.swan --stdlib "$ROOT/compiler/sil"
printf 'Swan-authored SIL generation, verification, demand, folding and DCE passed.\n'
