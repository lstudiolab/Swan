#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/std-json"
mkdir -p "$TMP"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$ROOT/runtime" \
    -c "$ROOT/BrewRuntime/json_abi.c" -o "$TMP/json_abi.o"
cat > "$TMP/json.brew" <<BREW
import C "$ROOT/BrewRuntime/json_abi.h"

func main() -> int:
    let document = brew_json_create(64)
    if document == 0:
        return 1
    if brew_json_parse_text(document, "{\"workers\":128,\"enabled\":true}") != 1:
        return 2
    if brew_json_get_i64(document, "workers", -1) != 128:
        return 3
    if brew_json_has_key(document, "enabled") != 1:
        return 4
    if brew_json_compact(document) == 0 or brew_json_compact_length(document) <= 0:
        return 5
    if brew_json_parse_text(document, "{") != 0:
        return 6
    if brew_json_error(document) == 0:
        return 7
    if brew_json_parse_text(document, "{\"workers\":64}") != 1:
        return 8
    if brew_json_get_i64(document, "workers", -1) != 64:
        return 9
    brew_json_close(document)
    return 0
BREW
"$ROOT/build/brewc" build "$TMP/json.brew" --link "$TMP/json_abi.o" -o "$TMP/json"
"$TMP/json"
printf 'PASS: Brew C ABI JSON parsing, lookup, compact encoding, and handle reuse\n'
