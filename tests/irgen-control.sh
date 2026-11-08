#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -I"$ROOT/compiler/include" "$ROOT/tests/irgen-control.c" -o "$ROOT/build/irgen-control-test"
"$ROOT/build/irgen-control-test"
