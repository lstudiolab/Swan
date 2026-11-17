#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/std-json"
mkdir -p "$TMP"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$ROOT/runtime" \
    -c "$ROOT/runtime/json_abi.c" -o "$TMP/json_abi.o"
cat > "$TMP/json.swan" <<'SWAN'
import std_json

func main() -> int:
    let document = !json_create()
    if document == 0:
        return 1
    if !json_parse(document, "{\"workers\":128,\"enabled\":true}") != 1:
        return 2
    if !json_get_i64(document, "workers", -1) != 128:
        return 3
    if !json_has_key(document, "enabled") != 1:
        return 4
    if !json_compact(document) == 0 or !json_compact_length(document) <= 0:
        return 5
    if !json_parse(document, "{") != 0:
        return 6
    if !json_error(document) == 0:
        return 7
    if !json_parse(document, "{\"workers\":64}") != 1:
        return 8
    if !json_get_i64(document, "workers", -1) != 64:
        return 9
    !json_close(document)
    return 0
SWAN
"$ROOT/build/swan" build "$TMP/json.swan" --link "$TMP/json_abi.o" -o "$TMP/json"
"$TMP/json"
printf 'PASS: Swan std_json import, JSON lookup, encoding, and error handles\n'
