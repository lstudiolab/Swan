#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$ROOT" \
    "$ROOT/tests/json-writer.c" "$ROOT/BrewRuntime/json_writer.c" \
    -o "$ROOT/build/json-writer"
"$ROOT/build/json-writer"

cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$ROOT" \
    -c "$ROOT/BrewRuntime/json_writer.c" -o "$ROOT/build/json-writer.o"
cat > "$ROOT/build/json-writer.brew" <<BREW
import C "$ROOT/BrewRuntime/json_writer.h"

func main() -> int:
    let writer = brew_json_writer_create(8)
    if writer == 0:
        return 1
    if brew_json_writer_begin_object(writer) != 1:
        return 2
    if brew_json_writer_key(writer, "message") != 1:
        return 3
    if brew_json_writer_string(writer, "Hello from Brew") != 1:
        return 4
    if brew_json_writer_end_object(writer) != 1:
        return 5
    if brew_json_writer_finish(writer) == 0:
        return 6
    if brew_json_writer_length(writer) <= 0:
        return 7
    brew_json_writer_destroy(writer)
    return 0
BREW
"$ROOT/build/brewc" build "$ROOT/build/json-writer.brew" \
    --link "$ROOT/build/json-writer.o" -o "$ROOT/build/json-writer-brew"
"$ROOT/build/json-writer-brew"
printf 'PASS: Brew JSON writer C ABI integration\n'
