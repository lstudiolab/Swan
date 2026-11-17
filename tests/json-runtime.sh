#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
cc -std=c11 -Wall -Wextra -Wpedantic -Werror -I"$ROOT" \
    "$ROOT/tests/json-runtime.c" -o "$ROOT/build/json-runtime"
"$ROOT/build/json-runtime"
