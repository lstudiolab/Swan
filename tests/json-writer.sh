#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$ROOT" \
    "$ROOT/tests/json-writer.c" "$ROOT/runtime/json_writer.c" \
    -o "$ROOT/build/json-writer"
"$ROOT/build/json-writer"

cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -I"$ROOT" \
    -c "$ROOT/runtime/json_writer.c" -o "$ROOT/build/json-writer.o"
cat > "$ROOT/build/json-writer.swan" <<SWAN
import C "$ROOT/runtime/json_writer.h"

func main() -> int:
    let writer = swan_json_writer_create(8)
    if writer == 0:
        return 1
    if swan_json_writer_begin_object(writer) != 1:
        return 2
    if swan_json_writer_key(writer, "message") != 1:
        return 3
    if swan_json_writer_string(writer, "Hello from Swan") != 1:
        return 4
    if swan_json_writer_end_object(writer) != 1:
        return 5
    if swan_json_writer_finish(writer) == 0:
        return 6
    if swan_json_writer_length(writer) <= 0:
        return 7
    swan_json_writer_destroy(writer)
    return 0
SWAN
"$ROOT/build/swan" build "$ROOT/build/json-writer.swan" \
    --link "$ROOT/build/json-writer.o" -o "$ROOT/build/json-writer-swan"
"$ROOT/build/json-writer-swan"
printf 'PASS: Swan JSON writer C ABI integration\n'
