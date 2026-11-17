#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -pthread -I"$ROOT" \
    "$ROOT/tests/runtime-primitives.c" -o "$ROOT/build/runtime-primitives"
"$ROOT/build/runtime-primitives"
