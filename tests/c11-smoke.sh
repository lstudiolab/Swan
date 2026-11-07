#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/c11-tests"
mkdir -p "$TMP"
cat > "$TMP/hello.swan" <<'SWAN'
fn main():
    log "Hello from Swan C"
    return 0
SWAN
"$SWAN" "$TMP/hello.swan" --emit-asm -o "$TMP/hello.s"
cc -x assembler "$TMP/hello.s" -o "$TMP/hello"
test "$("$TMP/hello")" = "Hello from Swan C"
printf 'PASS: C11 frontend emits native x86-64 assembly and string output\n'
cat > "$TMP/arithmetic.swan" <<'SWAN'
fn main():
    let a: int = 40
    var b: int = 2
    b = b + 1
    log a + b
    return 0
SWAN
"$SWAN" "$TMP/arithmetic.swan" --emit-asm -o "$TMP/arithmetic.s"
cc -x assembler "$TMP/arithmetic.s" -o "$TMP/arithmetic"
test "$("$TMP/arithmetic")" = "43"
printf 'PASS: typed locals, mutation, and arithmetic lowering\n'
cat > "$TMP/remainder.swan" <<'SWAN'
fn main():
    log 195 % 97
    log 194 % 97
    log 9223372036854775807 % 97
    log -195 % 97
    log 13 % 4
    log -13 % 4
    log -8 % 4
    return 0
SWAN
"$SWAN" "$TMP/remainder.swan" --emit-asm -o "$TMP/remainder.s"
cc -x assembler "$TMP/remainder.s" -o "$TMP/remainder"
test "$("$TMP/remainder")" = "1
0
78
-1
1
-1
0"
printf 'PASS: signed constant remainder lowering\n'
cat > "$TMP/calls.swan" <<'SWAN'
fn add(a: int, b: int) -> int:
    return a + b
fn main():
    log add(20, 22)
    return 0
SWAN
"$SWAN" "$TMP/calls.swan" --emit-asm -o "$TMP/calls.s"
cc -x assembler "$TMP/calls.s" -o "$TMP/calls"
test "$("$TMP/calls")" = "42"
printf 'PASS: function calls and System V argument registers\n'
cat > "$TMP/control.swan" <<'SWAN'
fn main():
    var value: int = 0
    while value < 3
        if value == 1
            log 9
        else
            log value
        value = value + 1
    return 0
SWAN
"$SWAN" "$TMP/control.swan" --emit-asm -o "$TMP/control.s"
cc -x assembler "$TMP/control.s" -o "$TMP/control"
test "$("$TMP/control")" = "0
9
2"
printf 'PASS: nested if/else and while control flow\n'
cat > "$TMP/unroll.swan" <<'SWAN'
fn main():
    var i = 0
    var sum = 0
    while i < 10
        sum = sum + i
        i = i + 1
    log sum
    return 0
SWAN
"$SWAN" "$TMP/unroll.swan" --emit-asm -o "$TMP/unroll.s"
cc -x assembler "$TMP/unroll.s" -o "$TMP/unroll"
test "$("$TMP/unroll")" = "45"
printf 'PASS: loop condition and induction-variable fast paths preserve iteration counts\n'
printf 'PASS: bottom-tested loop branches preserve iteration counts\n'
cat > "$TMP/nested-loops.swan" <<'SWAN'
fn main():
    var i = 0
    var total = 0
    while i < 3
        var j = 0
        while j < 4
            total = total + 1
            j = j + 1
        i = i + 1
    log total
    return 0
SWAN
"$SWAN" "$TMP/nested-loops.swan" --emit-asm -o "$TMP/nested-loops.s"
cc -x assembler "$TMP/nested-loops.s" -o "$TMP/nested-loops"
test "$("$TMP/nested-loops")" = "12"
printf 'PASS: nested bottom-tested loops\n'
cat > "$TMP/invariant-hoist.swan" <<'SWAN'
fn main():
    var i = 0
    var total = 0
    while i < 3
        var j = 0
        while j < 4
            var k = 0
            while k < 5
                total = total + ((i * 31 + j * 17 + k * 13) % 97)
                k = k + 1
            j = j + 1
        i = i + 1
    log total
    return 0
SWAN
"$SWAN" "$TMP/invariant-hoist.swan" --emit-asm -o "$TMP/invariant-hoist.s"
cc -x assembler "$TMP/invariant-hoist.s" -o "$TMP/invariant-hoist"
test "$("$TMP/invariant-hoist")" = "2913"
printf 'PASS: loop-invariant arithmetic hoisting preserves results\n'
cat > "$TMP/zero-loop.swan" <<'SWAN'
fn main():
    var i = 0
    var total = 0
    while i < 0
        total = total + 1
        i = i + 1
    log total
    return 0
SWAN
"$SWAN" "$TMP/zero-loop.swan" --emit-asm -o "$TMP/zero-loop.s"
cc -x assembler "$TMP/zero-loop.s" -o "$TMP/zero-loop"
test "$("$TMP/zero-loop")" = "0"
printf 'PASS: bottom-tested loop zero-iteration guard\n'
cat > "$TMP/loop-fallback.swan" <<'SWAN'
fn main():
    var i = 0
    var total = 0
    while i < 4
        if i == 1
            i = i + 1
        total = total + 1
        i = i + 1
    log total
    return 0
SWAN
"$SWAN" "$TMP/loop-fallback.swan" --emit-asm -o "$TMP/loop-fallback.s"
cc -x assembler "$TMP/loop-fallback.s" -o "$TMP/loop-fallback"
test "$("$TMP/loop-fallback")" = "3"
printf 'PASS: loops with noncanonical induction updates retain top-tested control flow\n'
cat > "$TMP/unknown-name.swan" <<'SWAN'
fn main():
    log missing_name
    return 0
SWAN
if "$SWAN" "$TMP/unknown-name.swan" --emit-asm -o "$TMP/unknown-name.s"; then
    echo 'FAIL: unknown variable was accepted' >&2
    exit 1
fi
printf 'PASS: semantic name resolution rejects unknown variables\n'
cat > "$TMP/immutable.swan" <<'SWAN'
fn main():
    let value = 1
    value = 2
    return 0
SWAN
if "$SWAN" "$TMP/immutable.swan" --emit-asm -o "$TMP/immutable.s"; then
    echo 'FAIL: assignment to immutable binding was accepted' >&2
    exit 1
fi
printf 'PASS: semantic analysis enforces immutable bindings\n'
cat > "$TMP/nested-calls.swan" <<'SWAN'
fn base() -> int:
    return 20
fn add(a: int, b: int) -> int:
    return a + b
fn wrap(value: int) -> int:
    return add(value, base())
fn main():
    log 1 + wrap(21)
    return 0
SWAN
"$SWAN" "$TMP/nested-calls.swan" --emit-asm -o "$TMP/nested-calls.s"
cc -x assembler "$TMP/nested-calls.s" -o "$TMP/nested-calls"
test "$("$TMP/nested-calls")" = "42"
printf 'PASS: nested function calls preserve x86-64 ABI stack alignment\n'
"$SWAN" --version | grep -q 'Swan 0.6.0'
printf 'PASS: C compiler version command\n'
