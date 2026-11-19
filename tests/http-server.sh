#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
mkdir -p "$ROOT/build"
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -I"$ROOT" \
    "$ROOT/tests/http-server.c" "$ROOT/runtime/http_server.c" \
    "$ROOT/runtime/json_abi.c" -o "$ROOT/build/http-server"
"$ROOT/build/http-server"

cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -I"$ROOT" \
    -c "$ROOT/runtime/http_json.c" -o "$ROOT/build/http-abi-runtime.o"
cat > "$ROOT/build/http-abi.brew" <<BREW
import C "$ROOT/runtime/http_server.h"

func main() -> int:
    let server = brew_http_json_server_create(0, 2, 8)
    if server == 0:
        return 1
    if brew_http_json_server_port(server) <= 0:
        return 2
    if brew_http_json_server_completed(server) != 0:
        return 3
    brew_http_json_server_destroy(server)
    return 0
BREW
"$ROOT/build/brew" build "$ROOT/build/http-abi.brew" \
    --link "$ROOT/build/http-abi-runtime.o" -o "$ROOT/build/http-abi"
"$ROOT/build/http-abi"
printf 'PASS: Brew HTTP server C ABI integration\n'
