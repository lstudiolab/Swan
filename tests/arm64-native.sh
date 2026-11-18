#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swanc"
case "$(uname -m)" in
    aarch64|arm64) ;;
    *) echo "SKIP: native ARM64 execution test requires an ARM64 runner"; exit 0 ;;
esac
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/i32.swan" <<'SWAN'
func combine(a: i32, b: i32) -> i32:
    var total: i32 = a + b
    total = total * 3
    total = total - 2
    let quotient: i32 = total / 3
    let remainder: i32 = total % 3
    return quotient + remainder
func main() -> i32:
    return combine(-7, 2) + 40
SWAN

# On ARM64, the host target is selected automatically and build links an executable.
"$BIN" build "$TMP/i32.swan" -o "$TMP/default-native"
test -x "$TMP/default-native"
set +e
"$TMP/default-native"
status=$?
set -e
test "$status" -eq 33
set +e
"$BIN" run "$TMP/i32.swan"
default_run_status=$?
set -e
test "$default_run_status" -eq 33

# Explicit native ARM64 build and run must use the same ABI and code path.
"$BIN" build "$TMP/i32.swan" --target arm64 -o "$TMP/explicit-native"
test -x "$TMP/explicit-native"
set +e
"$BIN" run "$TMP/i32.swan" --target arm64
run_status=$?
set -e
test "$run_status" -eq 33
printf 'PASS: native ARM64 default target, executable linking, i32 arithmetic, and execution\n'

cat > "$TMP/c_api.h" <<'H'
#include <stdint.h>
int32_t swan_arm64_add(int32_t left, int32_t right);
int64_t swan_arm64_sum9(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f, int64_t g, int64_t h, int64_t i);
H
cat > "$TMP/c_api.c" <<'C'
#include <stdint.h>
int32_t swan_arm64_add(int32_t left, int32_t right) {
    return left + right;
}
int64_t swan_arm64_sum9(int64_t a, int64_t b, int64_t c, int64_t d,
                        int64_t e, int64_t f, int64_t g, int64_t h, int64_t i) {
    return a + b + c + d + e + f + g + h + i;
}
C
cc -std=c11 -O2 -c "$TMP/c_api.c" -o "$TMP/c_api.o"
cat > "$TMP/ffi.swan" <<SWAN
import C "$TMP/c_api.h"
func main() -> i32:
    return swan_arm64_add(19, 23)
SWAN
"$BIN" build "$TMP/ffi.swan" --target arm64 --link "$TMP/c_api.o" -o "$TMP/ffi-native"
test -x "$TMP/ffi-native"
set +e
"$TMP/ffi-native"
ffi_status=$?
set -e
test "$ffi_status" -eq 42
printf 'PASS: native ARM64 C header import, i32 arguments, return values, and linking\n'

cat > "$TMP/stack-abi.swan" <<SWAN
import C "$TMP/c_api.h"
func main() -> int:
    return swan_arm64_sum9(1, 2, 3, 4, 5, 6, 7, 8, 9)
SWAN
"$BIN" build "$TMP/stack-abi.swan" --target arm64 --link "$TMP/c_api.o" -o "$TMP/stack-abi"
test -x "$TMP/stack-abi"
set +e
"$TMP/stack-abi"
stack_status=$?
set -e
test "$stack_status" -eq 45
printf 'PASS: native ARM64 C ABI stack-passed arguments\n'
