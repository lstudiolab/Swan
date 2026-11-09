#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
sh build.sh
OUTPUT="$ROOT/build/sil-pipeline"
rm -f "$OUTPUT"
"$ROOT/build/swan" build tests/sil_pipeline.swan --stdlib "$ROOT/compiler/sil" -o "$OUTPUT"
"$OUTPUT"
rm -f "$OUTPUT"
printf 'Native Swan-authored SIL generation, verification, folding, liveness and DCE passed.\n'
