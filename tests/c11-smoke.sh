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
if ! grep -q "mov r11, rax" "$TMP/invariant-hoist.s" || ! grep -q "mov rax, r11" "$TMP/invariant-hoist.s" || ! grep -q "imul r15, r14, 13" "$TMP/invariant-hoist.s" || ! grep -q "add r15, 13" "$TMP/invariant-hoist.s"; then
    echo "FAIL: expected invariant expression was not hoisted" >&2
    cat "$TMP/invariant-hoist.s" >&2
    exit 1
fi
cc -x assembler "$TMP/invariant-hoist.s" -o "$TMP/invariant-hoist"
actual=$("$TMP/invariant-hoist")
if [ "$actual" != "2913" ]; then echo "FAIL: invariant-hoist expected 2913, got $actual" >&2; exit 1; fi
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
cat > "$TMP/call-hotpath.swan" <<'SWAN'
fn transform(x: int) -> int:
    return (x * 31 + 7) % 97
fn main():
    var total = 0
    var i = 0
    while i < 8
        if i % 2 == 0
            total = total + transform(i)
        else
            total = total + transform(i + 1)
        i = i + 1
    log total
    return 0
SWAN
"$SWAN" "$TMP/call-hotpath.swan" --emit-asm -o "$TMP/call-hotpath.s"
cc -x assembler "$TMP/call-hotpath.s" -o "$TMP/call-hotpath"
test "$("$TMP/call-hotpath")" = "466"
grep -q 'test r12, 1' "$TMP/call-hotpath.s"
if grep -q 'call swan_fn_transform' "$TMP/call-hotpath.s"; then
    echo 'FAIL: eligible single-return function was not inlined' >&2
    exit 1
fi
grep -q 'imul rax' "$TMP/call-hotpath.s"
if grep -q 'mov QWORD PTR \[rsp' "$TMP/call-hotpath.s"; then
    echo 'FAIL: simple direct-call arguments were unnecessarily staged on the stack' >&2
    exit 1
fi
if grep -A8 '^swan_fn_transform:' "$TMP/call-hotpath.s" | grep -Eq 'push rbp|mov QWORD PTR \[rbp'; then
    echo 'FAIL: leaf function unexpectedly creates a stack frame' >&2
    exit 1
fi
if grep -B2 'call swan_fn_transform' "$TMP/call-hotpath.s" | grep -q 'push rax'; then
    echo 'FAIL: accumulator was spilled around a call' >&2
    exit 1
fi
printf 'PASS: leaf-call, register argument, accumulator, and parity fast paths\n'
cat > "$TMP/leaf-registers.swan" <<'SWAN'
fn leaf(a: int, b: int, c: int, d: int, e: int) -> int:
    return (a + b) * (c + d) + e
fn main():
    log leaf(1, 2, 3, 4, 5)
    return 0
SWAN
"$SWAN" "$TMP/leaf-registers.swan" --emit-asm -o "$TMP/leaf-registers.s"
cc -x assembler "$TMP/leaf-registers.s" -o "$TMP/leaf-registers"
actual=$( "$TMP/leaf-registers" )
if [ "$actual" != "26" ]; then echo "FAIL: leaf-registers expected 26, got $actual" >&2; cat "$TMP/leaf-registers.s" >&2; exit 1; fi
if grep -A12 '^swan_fn_leaf:' "$TMP/leaf-registers.s" | grep -Eq 'push rbp|mov QWORD PTR \[rbp'; then
    echo 'FAIL: five-register leaf function unexpectedly spills or creates a frame' >&2
    exit 1
fi
printf 'PASS: leaf volatile-register allocation does not overlap backend scratch registers\n'
cat > "$TMP/accumulator-side-effect.swan" <<'SWAN'
fn main():
    var total = 2
    total = total + (total = 5)
    log total
    return 0
SWAN
"$SWAN" "$TMP/accumulator-side-effect.swan" --emit-asm -o "$TMP/accumulator-side-effect.s"
cc -x assembler "$TMP/accumulator-side-effect.s" -o "$TMP/accumulator-side-effect"
test "$("$TMP/accumulator-side-effect")" = "7"
printf 'PASS: accumulator optimization preserves assignment evaluation order\n'
cat > "$TMP/constant-fold.swan" <<'SWAN'
fn main():
    log (6 * 7) + (9 - 2)
    return 0
SWAN
"$SWAN" "$TMP/constant-fold.swan" --emit-asm -o "$TMP/constant-fold.s"
cc -x assembler "$TMP/constant-fold.s" -o "$TMP/constant-fold"
test "$("$TMP/constant-fold")" = "49"
if grep -Eq 'imul|add rax|sub rax' "$TMP/constant-fold.s"; then
    echo 'FAIL: constant expression was not folded before assembly emission' >&2
    cat "$TMP/constant-fold.s" >&2
    exit 1
fi
printf 'PASS: integer constant folding removes runtime arithmetic\n'
"$SWAN" --version | grep -q 'Swan 0.6.0'
printf 'PASS: C compiler version command\n'
