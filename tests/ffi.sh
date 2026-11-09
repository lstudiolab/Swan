#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/ffi"
mkdir -p "$TMP"
CC=${CC:-cc}

cat > "$TMP/c_api.h" <<'H'
#include <stdint.h>
int64_t swan_c_add(int64_t left, int64_t right);
void swan_c_noop(int64_t value);
H

cat > "$TMP/c_library.c" <<'C'
#include <stdint.h>
int64_t swan_c_add(int64_t left, int64_t right) {
    return left + right;
}
void swan_c_noop(int64_t value) { (void)value; }
C
"$CC" -std=c11 -O2 -c "$TMP/c_library.c" -o "$TMP/c_library.o"

cat > "$TMP/ffi.swan" <<'SWAN'
import C "$TMP/c_api.h"
fn main() -> int:
    swan_c_noop(7)
    return swan_c_add(19, 23)
SWAN
"$SWAN" build "$TMP/ffi.swan" --link "$TMP/c_library.o" -o "$TMP/ffi"
set +e
"$TMP/ffi"
status=$?
set -e
test "$status" -eq 42
printf 'PASS: SIL/IR external C ABI call and object linking\n'

"$SWAN" "$TMP/ffi.swan" --emit-asm -o "$TMP/ffi.s"
grep -q '^.extern swan_c_add$' "$TMP/ffi.s"
grep -q 'call swan_c_add' "$TMP/ffi.s"
printf 'PASS: external symbol preserved in native assembly\n'

cat > "$TMP/missing.swan" <<'SWAN'
extern fn swan_missing(value: i64) -> i64
fn main() -> int:
    return swan_missing(1)
SWAN
if "$SWAN" build "$TMP/missing.swan" -o "$TMP/missing" >"$TMP/missing.out" 2>&1; then
    echo 'FAIL: unresolved external symbol linked successfully' >&2
    exit 1
fi
grep -Eq 'undefined reference|unresolved external' "$TMP/missing.out"
printf 'PASS: unresolved external symbols fail at link time\n'
