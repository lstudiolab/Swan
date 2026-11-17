#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/std-json"
mkdir -p "$TMP"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$ROOT/runtime" \
    -c "$ROOT/runtime/json_abi.c" -o "$TMP/json_abi.o"
cat > "$TMP/json.swan" <<SWAN
import C "$ROOT/runtime/json_abi.h"

func main() -> int:
    let document = swan_json_create(64)
    if document == 0:
        return 1
    if swan_json_parse_text(document, "{\"workers\":128,\"enabled\":true}") != 1:
        return 2
    if swan_json_get_i64(document, "workers", -1) != 128:
        return 3
    if swan_json_has_key(document, "enabled") != 1:
        return 4
    if swan_json_compact(document) == 0 or swan_json_compact_length(document) <= 0:
        return 5
    if swan_json_parse_text(document, "{") != 0:
        return 6
    if swan_json_error(document) == 0:
        return 7
    if swan_json_parse_text(document, "{\"workers\":64}") != 1:
        return 8
    if swan_json_get_i64(document, "workers", -1) != 64:
        return 9
    swan_json_close(document)
    return 0
SWAN
"$ROOT/build/swan" build "$TMP/json.swan" --link "$TMP/json_abi.o" -o "$TMP/json"
"$TMP/json"
printf 'PASS: Swan C ABI JSON parsing, lookup, compact encoding, and handle reuse\n'
