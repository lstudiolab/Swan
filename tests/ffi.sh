#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brew"
TMP="$ROOT/build/ffi"
mkdir -p "$TMP"
CC=cc

cat > "$TMP/c_api.h" <<'H'
#include <stdint.h>
#include <stddef.h>
int64_t brew_c_add(int64_t left, int64_t right);
void brew_c_noop(int64_t value);
int32_t brew_c_double(int32_t value);
void *brew_c_identity(void *value);
size_t brew_c_strlen(const char *value);
int32_t brew_c_string_eq(const char *left, const char *right);
H

cat > "$TMP/c_library.c" <<'C'
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "json_abi.c"
int64_t brew_c_add(int64_t left, int64_t right) { return left + right; }
void brew_c_noop(int64_t value) { (void)value; }
int32_t brew_c_double(int32_t value) { return value * 2; }
void *brew_c_identity(void *value) { return value; }
size_t brew_c_strlen(const char *value) { return strlen(value); }
int32_t brew_c_string_eq(const char *left, const char *right) {
    return strcmp(left, right) == 0;
}
C
"$CC" -std=c11 -O2 -I"$ROOT/runtime" -c "$TMP/c_library.c" -o "$TMP/c_library.o"

cat > "$TMP/ffi.brew" <<BREW
import C "$TMP/c_api.h"
import C "$ROOT/runtime/json_abi.h"
func sil_identity(value: i64) -> i64:
    return value

func main() -> i32:
    brew_c_noop(7)
    brew_c_add(sil_identity(19), 23)
    if brew_c_identity(42) != 42:
        return 3
    let greeting = "Brew"
    if brew_c_strlen(greeting) != 4:
        return 4
    if brew_c_string_eq(greeting, "Brew") != 1:
        return 5
    let json = "{\"workers\":128,\"enabled\":true}"
    let document = brew_json_create(64)
    if document == 0:
        return 6
    if brew_json_parse_text(document, json) != 1:
        return 6
    if brew_json_get_i64(document, "workers", -1) != 128:
        return 7
    if brew_json_has_key(document, "enabled") != 1:
        return 8
    let encoded = brew_json_compact(document)
    if brew_c_strlen(encoded) < 10:
        return 9
    brew_json_close(document)
    return brew_c_double(21)
BREW
"$BREW" build "$TMP/ffi.brew" --link "$TMP/c_library.o" -o "$TMP/ffi"
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

"$BREW" "$TMP/ffi.brew" --emit-asm -o "$TMP/ffi.s"
grep -q '^.extern brew_c_add$' "$TMP/ffi.s"
grep -q '^.extern brew_c_noop$' "$TMP/ffi.s"
grep -q '^.extern brew_c_double$' "$TMP/ffi.s"
grep -q '^.extern brew_c_strlen$' "$TMP/ffi.s"
grep -q '^.extern brew_c_string_eq$' "$TMP/ffi.s"
grep -q '^.extern brew_json_create$' "$TMP/ffi.s"
grep -q '^.extern brew_json_parse_text$' "$TMP/ffi.s"
grep -q '^.extern brew_json_get_i64$' "$TMP/ffi.s"
grep -q '^.extern brew_json_compact$' "$TMP/ffi.s"
grep -q 'call brew_c_add' "$TMP/ffi.s"
grep -q 'call brew_c_noop' "$TMP/ffi.s"
grep -q 'call brew_c_double' "$TMP/ffi.s"
grep -q 'call brew_c_strlen' "$TMP/ffi.s"
grep -q 'call brew_c_string_eq' "$TMP/ffi.s"
grep -q 'call brew_json_create' "$TMP/ffi.s"
grep -q 'call brew_json_parse_text' "$TMP/ffi.s"
grep -q 'call brew_json_get_i64' "$TMP/ffi.s"
grep -q 'call brew_json_compact' "$TMP/ffi.s"
grep -q '\.Lbrew_string_' "$TMP/ffi.s"
printf 'PASS: imported C symbols and string literal storage emitted in native assembly\n'

cat > "$TMP/missing.brew" <<'BREW'
external func brew_missing(value: i64) -> i64
func main() -> int:
    return brew_missing(1)
BREW
if "$BREW" build "$TMP/missing.brew" -o "$TMP/missing" >"$TMP/missing.out" 2>&1; then
    echo 'FAIL: unresolved external symbol linked successfully' >&2
    exit 1
fi
grep -Eq 'undefined reference|unresolved external' "$TMP/missing.out"
printf 'PASS: unresolved external symbols fail at link time\n'
