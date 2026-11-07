#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
sh "$ROOT/build.sh"
sh "$ROOT/tests/smoke.sh"
sh "$ROOT/tests/regressions.sh"
sh "$ROOT/tests/architecture.sh"
sh "$ROOT/tests/c-compiler.sh"
