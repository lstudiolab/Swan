#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -I"$ROOT" \
    "$ROOT/tests/http-server.c" "$ROOT/runtime/http_server.c" \
    "$ROOT/runtime/json_abi.c" -o "$ROOT/build/http-server"
"$ROOT/build/http-server"
