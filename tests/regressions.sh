#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/regressions"
mkdir -p "$TMP"

expect_rejected() {
    name=$1
    if "$SWAN" build "$TMP/$name.swan" -o "$TMP/$name" >/dev/null 2>&1; then
        echo "FAIL: $name should have been rejected" >&2
        exit 1
    fi
    printf 'PASS: rejected %s\n' "$name"
}

cat > "$TMP/break-outside-loop.swan" <<'SWAN'
fn main():
    break
    return 0
SWAN
expect_rejected break-outside-loop

cat > "$TMP/continue-outside-loop.swan" <<'SWAN'
fn main():
    continue
    return 0
SWAN
expect_rejected continue-outside-loop

cat > "$TMP/immutable-assignment.swan" <<'SWAN'
fn main():
    let value = 1
    value = 2
    return 0
SWAN
expect_rejected immutable-assignment

cat > "$TMP/unknown-name.swan" <<'SWAN'
fn main():
    log missing_value
    return 0
SWAN
expect_rejected unknown-name

cat > "$TMP/unknown-function.swan" <<'SWAN'
fn main():
    missing_function()
    return 0
SWAN
expect_rejected unknown-function

cat > "$TMP/call-arity.swan" <<'SWAN'
fn add(left: int, right: int) -> int:
    return left + right
fn main():
    log add(1)
    return 0
SWAN
expect_rejected call-arity

cat > "$TMP/missing-import.swan" <<'SWAN'
import module_that_does_not_exist
fn main():
    return 0
SWAN
expect_rejected missing-import

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
"$SWAN" build "$TMP/return-both-branches.swan" -o "$TMP/return-both-branches"
test "$("$TMP/return-both-branches")" = "11"
printf 'PASS: function calls and branch return lowering\n'

cat > "$TMP/short-circuit.swan" <<'SWAN'
fn main():
    if false and (1 / 0 == 0)
        log 1
    if true or (1 / 0 == 0)
        log 2
    return 0
SWAN
"$SWAN" build "$TMP/short-circuit.swan" -o "$TMP/short-circuit"
test "$("$TMP/short-circuit")" = "2"
printf 'PASS: logical operators short-circuit at runtime\n'

cat > "$TMP/loop-control.swan" <<'SWAN'
fn main():
    var i = 0
    var total = 0
    while i < 10
        i = i + 1
        if i % 2 == 0
            continue
        total = total + i
        if total > 10
            break
    log total
    return 0
SWAN
"$SWAN" build "$TMP/loop-control.swan" -o "$TMP/loop-control"
test "$("$TMP/loop-control")" = "16"
printf 'PASS: nested break and continue control flow\n'

cat > "$TMP/array-modes.swan" <<'SWAN'
fn main():
    let checked = array(4)
    let unchecked = array_unchecked(4)
    var i = 0
    while i < 4
        checked[i] = i + 1
        unchecked[i] = i + 5
        i = i + 1
    log checked[3]
    log unchecked[3]
    return 0
SWAN
"$SWAN" build "$TMP/array-modes.swan" -o "$TMP/array-modes"
test "$("$TMP/array-modes")" = "4
8"
printf 'PASS: checked and unchecked contiguous u32 arrays\n'

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
"$SWAN" build "$TMP/signed-remainder.swan" -o "$TMP/signed-remainder"
test "$("$TMP/signed-remainder")" = "50000"
printf 'PASS: signed constant remainder lowering\n'

printf 'PASS: compiler regression suite\n'
