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

# Explicit native ARM64 build and run must use the same ABI and code path.
"$BIN" build "$TMP/i32.swan" --target arm64 -o "$TMP/explicit-native"
test -x "$TMP/explicit-native"
set +e
"$BIN" run "$TMP/i32.swan" --target arm64
run_status=$?
set -e
test "$run_status" -eq 33
printf 'PASS: native ARM64 default target, executable linking, i32 arithmetic, and execution\n'
