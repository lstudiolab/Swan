#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
sh build.sh
if "$ROOT/build/swan" run tests/sil_pipeline.swan --stdlib "$ROOT/compiler/sil"; then
    printf 'Swan-authored SIL generation, verification, demand, folding and DCE passed.\n'
else
    status=$?
    printf 'Swan-authored SIL pipeline failed with exit code %s.\n' "$status" >&2
    exit "$status"
fi
