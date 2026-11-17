#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/ffi"
mkdir -p "$TMP"
CC=cc

cat > "$TMP/c_api.h" <<'H'
#include <stdint.h>
#include <stddef.h>
int64_t swan_c_add(int64_t left, int64_t right);
void swan_c_noop(int64_t value);
int32_t swan_c_double(int32_t value);
void *swan_c_identity(void *value);
size_t swan_c_strlen(const char *value);
int32_t swan_c_string_eq(const char *left, const char *right);
H

cat > "$TMP/c_library.c" <<'C'
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "json_abi.c"
int64_t swan_c_add(int64_t left, int64_t right) { return left + right; }
void swan_c_noop(int64_t value) { (void)value; }
int32_t swan_c_double(int32_t value) { return value * 2; }
void *swan_c_identity(void *value) { return value; }
size_t swan_c_strlen(const char *value) { return strlen(value); }
int32_t swan_c_string_eq(const char *left, const char *right) {
    return strcmp(left, right) == 0;
}
C
"$CC" -std=c11 -O2 -I"$ROOT/runtime" -c "$TMP/c_library.c" -o "$TMP/c_library.o"

cat > "$TMP/ffi.swan" <<SWAN
import C "$TMP/c_api.h"
import C "$ROOT/runtime/json_abi.h"
func sil_identity(value: i64) -> i64:
    return value

func main() -> i32:
    swan_c_noop(7)
    swan_c_add(sil_identity(19), 23)
    if swan_c_identity(42) != 42:
        return 3
    let greeting = "Swan"
    if swan_c_strlen(greeting) != 4:
        return 4
    if swan_c_string_eq(greeting, "Swan") != 1:
        return 5
    let json = "{\"workers\":128,\"enabled\":true}"
    let document = swan_json_create(64)
    if document == 0:
        return 6
    if swan_json_parse_text(document, json) != 1:
        return 6
    if swan_json_get_i64(document, "workers", -1) != 128:
        return 7
    if swan_json_has_key(document, "enabled") != 1:
        return 8
    let encoded = swan_json_compact(document)
    if swan_c_strlen(encoded) < 10:
        return 9
    swan_json_close(document)
    return swan_c_double(21)
SWAN
"$SWAN" build "$TMP/ffi.swan" --link "$TMP/c_library.o" -o "$TMP/ffi"
set +e
"$TMP/ffi"
status=$?
set -e
if [ "$status" -ne 42 ]; then
    echo "FAIL: linked FFI executable returned $status; expected 42" >&2
    objdump -d "$TMP/ffi" | tail -100 >&2 || true
    exit 1
fi
printf 'PASS: C header import, string pointers, JSON runtime, and native object linking\n'

"$SWAN" "$TMP/ffi.swan" --emit-asm -o "$TMP/ffi.s"
grep -q '^.extern swan_c_add$' "$TMP/ffi.s"
grep -q '^.extern swan_c_noop$' "$TMP/ffi.s"
grep -q '^.extern swan_c_double$' "$TMP/ffi.s"
grep -q '^.extern swan_c_strlen$' "$TMP/ffi.s"
grep -q '^.extern swan_c_string_eq$' "$TMP/ffi.s"
grep -q '^.extern swan_json_create$' "$TMP/ffi.s"
grep -q '^.extern swan_json_parse_text$' "$TMP/ffi.s"
grep -q '^.extern swan_json_get_i64$' "$TMP/ffi.s"
grep -q '^.extern swan_json_compact$' "$TMP/ffi.s"
grep -q 'call swan_c_add' "$TMP/ffi.s"
grep -q 'call swan_c_noop' "$TMP/ffi.s"
grep -q 'call swan_c_double' "$TMP/ffi.s"
grep -q 'call swan_c_strlen' "$TMP/ffi.s"
grep -q 'call swan_c_string_eq' "$TMP/ffi.s"
grep -q 'call swan_json_create' "$TMP/ffi.s"
grep -q 'call swan_json_parse_text' "$TMP/ffi.s"
grep -q 'call swan_json_get_i64' "$TMP/ffi.s"
grep -q 'call swan_json_compact' "$TMP/ffi.s"
grep -q '\.Lswan_string_' "$TMP/ffi.s"
printf 'PASS: imported C symbols and string literal storage emitted in native assembly\n'

cat > "$TMP/missing.swan" <<'SWAN'
external func swan_missing(value: i64) -> i64
func main() -> int:
    return swan_missing(1)
SWAN
if "$SWAN" build "$TMP/missing.swan" -o "$TMP/missing" >"$TMP/missing.out" 2>&1; then
    echo 'FAIL: unresolved external symbol linked successfully' >&2
    exit 1
fi
grep -Eq 'undefined reference|unresolved external' "$TMP/missing.out"
printf 'PASS: unresolved external symbols fail at link time\n'
