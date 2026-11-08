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
grep -Eq 'test (r12|rdi|rsi|rdx|r8|r9), 1' "$TMP/call-hotpath.s"
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
cat > "$TMP/local-operands.swan" <<'SWAN'
fn main():
    var left = 84
    var right = 7
    log left + right
    log left - right
    log left * right
    log left / right
    log left % right
    log left > right
    return 0
SWAN
"$SWAN" "$TMP/local-operands.swan" --emit-asm -o "$TMP/local-operands.s"
cc -x assembler "$TMP/local-operands.s" -o "$TMP/local-operands"
test "$("$TMP/local-operands")" = "91
77
588
12
0
1"
if grep -q 'push rax' "$TMP/local-operands.s"; then
    echo 'FAIL: local-operand arithmetic unnecessarily spills expression values' >&2
    cat "$TMP/local-operands.s" >&2
    exit 1
fi
printf 'PASS: register/memory local operands avoid expression stack temporaries\n'
cat > "$TMP/array-u32.swan" <<'SWAN'
fn main():
    let values = array_u32(4)
    values[0] = 4294967295
    values[1] = 42
    values[2] = 7
    values[3] = 99
    log values[0]
    log values[1]
    log values[2]
    log values[3]
    return 0
SWAN
"$SWAN" "$TMP/array-u32.swan" --emit-asm -o "$TMP/array-u32.s"
cc -x assembler "$TMP/array-u32.s" -o "$TMP/array-u32"
test "$("$TMP/array-u32")" = "4294967295
42
7
99"
printf 'PASS: zero-initialized u32 arrays, indexed loads/stores, and unsigned value preservation\n'
cat > "$TMP/array-midpoint.swan" <<'SWAN'
fn main():
    let values = array(8)
    var low = 0
    var high = 7
    while low <= high
        var middle = low + (high - low) // 2
        log values[middle]
        break
    return 0
SWAN
"$SWAN" "$TMP/array-midpoint.swan" --emit-asm -o "$TMP/array-midpoint.s"
cc -x assembler "$TMP/array-midpoint.s" -o "$TMP/array-midpoint"
test "$("$TMP/array-midpoint")" = "0"
grep -q 'shr rax, 1' "$TMP/array-midpoint.s"
if grep -Eq 'cmp rcx, QWORD PTR \[[a-z0-9]+ - 8\]' "$TMP/array-midpoint.s"; then
    echo "FAIL: proven midpoint array access retained a bounds check" >&2
    exit 1
fi
if grep -q 'mov r11, rbx' "$TMP/array-midpoint.s"; then
    echo 'FAIL: register-resident array base used an unnecessary scratch move' >&2
    exit 1
fi
printf 'PASS: proven-nonnegative midpoint and direct array-base addressing\n'

cat > "$TMP/unsafe-midpoint.swan" <<'SWAN'
fn main():
    let values = array(8)
    var low = 0
    var high = 7
    while low <= high
        var middle = low + (high - low) // 2
        log values[middle]
        low = -1
        break
    return 0
SWAN
"$SWAN" "$TMP/unsafe-midpoint.swan" --emit-asm -o "$TMP/unsafe-midpoint.s"
if ! grep -Eq 'cmp rcx, (8|QWORD PTR \[[a-z0-9]+ - 8\])' "$TMP/unsafe-midpoint.s"; then
    echo "FAIL: midpoint bounds check was removed after a bound mutation" >&2
    cat "$TMP/unsafe-midpoint.s" >&2
    exit 1
fi
printf 'PASS: midpoint bounds-check elimination rejects mutated bounds\n'

cat > "$TMP/power-two-multiply.swan" <<'SWAN'
fn main():
    var value = 7
    var dividend = -43
    log value * 8
    log -value * 8
    log value * -8
    log value * 1
    log value * -1
    log value * 0
    log dividend / 8
    log dividend / -8
    log dividend % 8
    log dividend % -8
    return 0
SWAN
"$SWAN" "$TMP/power-two-multiply.swan" --emit-asm -o "$TMP/power-two-multiply.s"
cc -x assembler "$TMP/power-two-multiply.s" -o "$TMP/power-two-multiply"
test "$("$TMP/power-two-multiply")" = "56
-56
-56
7
-7
0
-5
5
-3
-3"
grep -q 'shl rax, 3' "$TMP/power-two-multiply.s"
grep -q 'neg rax' "$TMP/power-two-multiply.s"
if grep -q 'idiv' "$TMP/power-two-multiply.s"; then
    echo "FAIL: signed power-of-two division/remainder used idiv" >&2
    exit 1
fi
printf 'PASS: signed power-of-two multiplication, division, and remainder lowering\n'

cat > "$TMP/array-store-register.swan" <<'SWAN'
fn main():
    let values = array_unchecked(4)
    var index = 0
    var first = 11
    var second = 22
    var third = 33
    var fourth = 44
    var fifth = 55
    var sixth = 66
    var seventh = 77
    while index < 4
        values[index] = fourth
        index = index + 1
    log first + second + third + fourth + fifth + sixth + seventh
    log values[3]
    return 0
SWAN
"$SWAN" "$TMP/array-store-register.swan" --emit-asm -o "$TMP/array-store-register.s"
cc -x assembler "$TMP/array-store-register.s" -o "$TMP/array-store-register"
test "$("$TMP/array-store-register")" = "308
44"
printf 'PASS: array stores preserve live allocated registers\n'

cat > "$TMP/add-scratch-preservation.swan" <<'SWAN'
fn main():
    let a = 1
    let b = 2
    let c = 3
    let d = 4
    let e = 5
    let f = 6
    let g = 7
    let h = 8
    let i = 9
    let j = 10
    let values = array(4)
    values[0] = 12
    var index = 0
    log a + values[index] + (b % 97)
    return 0
SWAN
"$SWAN" "$TMP/add-scratch-preservation.swan" --emit-asm -o "$TMP/add-scratch-preservation.s"
cc -x assembler "$TMP/add-scratch-preservation.s" -o "$TMP/add-scratch-preservation"
test "$("$TMP/add-scratch-preservation")" = "15"
printf 'PASS: addition accumulators survive spilled-array and remainder scratch use\n'

cat > "$TMP/heapsort.swan" <<'SWAN'
fn main():
    let values = array_u32_unchecked(10)
    values[0] = 91
    values[1] = 7
    values[2] = 42
    values[3] = 3
    values[4] = 88
    values[5] = 16
    values[6] = 55
    values[7] = 1
    values[8] = 73
    values[9] = 29
    var start = 4
    while start >= 0
        var root = start
        var active = 1
        while active == 1
            var child = root * 2 + 1
            if child < 10
                if child + 1 < 10
                    if values[child] < values[child + 1]
                        child = child + 1
                if values[root] < values[child]
                    var temp = values[root]
                    values[root] = values[child]
                    values[child] = temp
                    root = child
                else
                    active = 0
            else
                active = 0
        start = start - 1
    var end = 9
    while end > 0
        var temp2 = values[0]
        values[0] = values[end]
        values[end] = temp2
        var root2 = 0
        var active2 = 1
        while active2 == 1
            var child2 = root2 * 2 + 1
            if child2 < end
                if child2 + 1 < end
                    if values[child2] < values[child2 + 1]
                        child2 = child2 + 1
                if values[root2] < values[child2]
                    var temp3 = values[root2]
                    values[root2] = values[child2]
                    values[child2] = temp3
                    root2 = child2
                else
                    active2 = 0
            else
                active2 = 0
        end = end - 1
    var sorted = 1
    var i = 1
    while i < 10
        if values[i - 1] > values[i]
            sorted = 0
        i = i + 1
    log sorted
    return 0
SWAN
"$SWAN" "$TMP/heapsort.swan" --emit-asm -o "$TMP/heapsort.s"
cc -x assembler "$TMP/heapsort.s" -o "$TMP/heapsort"
test "$("$TMP/heapsort")" = "1"
printf 'PASS: in-place heapsort with unchecked u32 array indexing\n'

"$SWAN" "$ROOT/benchmarks/sort_1b.swan" --emit-asm -o "$TMP/sort-1b.s"
cc -x assembler "$TMP/sort-1b.s" -o "$TMP/sort-1b"
printf 'PASS: one-billion-element Swan sorting workload compiles and links\n'




cat > "$TMP/volatile-preservation.swan" <<'SWAN'
fn combine(a: int, b: int) -> int:
    return a + b
fn main():
    let a = 11
    let b = 22
    let c = 33
    let d = 44
    let e = 55
    let f = 66
    let g = 77
    log combine(a, 1) + combine(b, 1) + combine(c, 1) + combine(d, 1) + combine(e, 1) + combine(f, 1) + combine(g, 1)
    return 0
SWAN
"$SWAN" "$TMP/volatile-preservation.swan" --emit-asm -o "$TMP/volatile-preservation.s"
cc -x assembler "$TMP/volatile-preservation.s" -o "$TMP/volatile-preservation"
test "$("$TMP/volatile-preservation")" = "315"
grep -q 'mov QWORD PTR \[rbp - .*\], rbx' "$TMP/volatile-preservation.s"
grep -q 'mov QWORD PTR \[rbp - .*\], r12' "$TMP/volatile-preservation.s"
if ! grep -q 'push rdi' "$TMP/volatile-preservation.s" || ! grep -q 'pop rdi' "$TMP/volatile-preservation.s"; then
    echo 'FAIL: volatile locals were not preserved around function calls' >&2
    exit 1
fi
printf 'PASS: callee-saved and volatile local preservation across calls\n'
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
cat > "$TMP/local-compare.swan" <<'SWAN'
fn main():
    var index = 0
    let limit = 5
    while index < limit
        index = index + 1
    log index
    return 0
SWAN
"$SWAN" "$TMP/local-compare.swan" --emit-asm -o "$TMP/local-compare.s"
cc -x assembler "$TMP/local-compare.s" -o "$TMP/local-compare"
test "$("$TMP/local-compare")" = "5"
if ! grep -Eq 'cmp (rbx|r12|r13|r14|r15|rdi|rsi|rdx|r8|r9), (rbx|r12|r13|r14|r15|rdi|rsi|rdx|r8|r9)' "$TMP/local-compare.s"; then
    echo 'FAIL: local-to-local loop comparison did not use register operands' >&2
    cat "$TMP/local-compare.s" >&2
    exit 1
fi
printf 'PASS: local-to-local loop comparisons avoid stack temporaries\n'
"$SWAN" --version | grep -q 'Swan 0.7.0-samc'
printf 'PASS: C compiler version command\n'

cat > "$TMP/compound-arithmetic.swan" <<'SWAN'
fn main():
    var value = 6
    var factor = 7
    value = value * factor
    value = value - 2
    value = value + 1
    log value
    value = value * 8
    log value
    var a = 1
    var b = 2
    var c = 3
    var d = 4
    var e = 5
    var spilled = 6
    spilled = spilled * factor
    spilled = spilled - value
    log spilled
    return 0
SWAN
"$SWAN" "$TMP/compound-arithmetic.swan" --emit-asm -o "$TMP/compound-arithmetic.s"
cc -x assembler "$TMP/compound-arithmetic.s" -o "$TMP/compound-arithmetic"
test "$("$TMP/compound-arithmetic")" = "41
328
-286"
grep -q 'imul rbx, r12' "$TMP/compound-arithmetic.s"
grep -q 'shl rbx, 3' "$TMP/compound-arithmetic.s"
printf 'PASS: compound arithmetic updates register and spilled locals in place\n'
