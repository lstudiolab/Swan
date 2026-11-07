#!/usr/bin/env sh
set -eu
# Exercise the low-latency direct x86-64 backend explicitly.
export SWAN_FAST_COMPILE=1
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/regressions"
CC=${CC:-cc}
mkdir -p "$TMP"
expect_rejected() {
    name=$1
    if "$SWAN" "$TMP/$name.swan" -o "$TMP/$name.s" >/dev/null 2>&1; then
        echo "FAIL: $name should have been rejected" >&2
        exit 1
    fi
    printf 'PASS: rejected %s\n' "$name"
}
cat > "$TMP/missing-return.swan" <<'SWAN'
fn calculate(flag: bool) -> int:
    if flag
        return 1
fn main():
    return 0
SWAN
expect_rejected missing-return

cat > "$TMP/void-parameter.swan" <<'SWAN'
fn invalid(value: void) -> int:
    return 0
fn main():
    return 0
SWAN
expect_rejected void-parameter

cat > "$TMP/return-both-branches.swan" <<'SWAN'
fn choose(flag: bool) -> int:
    if flag
        return 11
    else
        return 22
fn main():
    log choose(true)
    return 0
SWAN
"$SWAN" "$TMP/return-both-branches.swan" -o "$TMP/return-both-branches.s"
"$CC" -x assembler "$TMP/return-both-branches.s" -o "$TMP/return-both-branches"
test "$("$TMP/return-both-branches")" = "11"
printf 'PASS: definite return across both branches\n'

cat > "$TMP/forward-struct.swan" <<'SWAN'
struct Bird:
    wing: Wing
struct Wing:
    size: int
fn main():
    return 0
SWAN
"$SWAN" "$TMP/forward-struct.swan" -o "$TMP/forward-struct.s"
"$CC" -x assembler "$TMP/forward-struct.s" -o "$TMP/forward-struct"
printf 'PASS: forward struct dependency ordering\n'

cat > "$TMP/struct-cycle.swan" <<'SWAN'
struct First:
    second: Second
struct Second:
    first: First
fn main():
    return 0
SWAN
expect_rejected struct-cycle

cat > "$TMP/immutable-string.swan" <<'SWAN'
fn echo(value: string) -> string:
    return value
fn main():
    let greeting = "Hello"
    log greeting
    log echo(greeting)
    return 0
SWAN
"$SWAN" "$TMP/immutable-string.swan" -o "$TMP/immutable-string.s"
"$CC" -x assembler "$TMP/immutable-string.s" -o "$TMP/immutable-string"
test "$("$TMP/immutable-string")" = "Hello
Hello"
printf 'PASS: immutable string and parameter declarations\n'

cat > "$TMP/signed-remainder.swan" <<'SWAN'
fn main():
    var value = -1000
    var total = 50000
    while value <= 1000
        total = total + (value % 97)
        value = value + 1
    log total
    return 0
SWAN
"$SWAN" "$TMP/signed-remainder.swan" -o "$TMP/signed-remainder.s"
"$CC" -x assembler "$TMP/signed-remainder.s" -o "$TMP/signed-remainder"
test "$("$TMP/signed-remainder")" = "50000"
printf 'PASS: branchless signed constant remainder lowering\n'

printf 'PASS: compiler regression suite\n'
