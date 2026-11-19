#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brewc"
TMP="$ROOT/build/regressions"
mkdir -p "$TMP"

expect_rejected() {
    name=$1
    if "$BREW" build "$TMP/$name.brew" -o "$TMP/$name" >/dev/null 2>&1; then
        echo "FAIL: $name should have been rejected" >&2
        exit 1
    fi
    printf 'PASS: rejected %s\n' "$name"
}

cat > "$TMP/break-outside-loop.brew" <<'BREW'
fn main():
    break
    return 0
BREW
expect_rejected break-outside-loop

cat > "$TMP/obsolete-out-terminator.brew" <<'BREW'
func main() -> int:
    out
    return 0
BREW
expect_rejected obsolete-out-terminator

cat > "$TMP/removed-printin.brew" <<'BREW'
func main() -> int:
    !printin >> "old syntax";
    return 0
BREW
expect_rejected removed-printin

cat > "$TMP/removed-bang-call.brew" <<'BREW'
import std
func main() -> int:
    print(!abs(-9))
    return 0
BREW
expect_rejected removed-bang-call

cat > "$TMP/continue-outside-loop.brew" <<'BREW'
fn main():
    continue
    return 0
BREW
expect_rejected continue-outside-loop

cat > "$TMP/immutable-assignment.brew" <<'BREW'
fn main():
    let value = 1
    value = 2
    return 0
BREW
expect_rejected immutable-assignment

cat > "$TMP/unknown-name.brew" <<'BREW'
fn main():
    print(missing_value)
    return 0
BREW
expect_rejected unknown-name

cat > "$TMP/index-scalar.brew" <<'BREW'
fn main():
    let value = 7
    print(value[0])
    return 0
BREW
expect_rejected index-scalar

cat > "$TMP/uninitialized-variable.brew" <<'BREW'
fn main():
    var value: int
    print(value)
    return 0
BREW
expect_rejected uninitialized-variable

cat > "$TMP/ambiguous-branch-shadowing.brew" <<'BREW'
fn main():
    if true:
        let value = 1
    else:
        let value = 2
    return 0
BREW
expect_rejected ambiguous-branch-shadowing

{
    printf 'fn main():\n    print('
    i=0
    while [ "$i" -lt 300 ]; do printf '('; i=$((i + 1)); done
    printf '1'
    i=0
    while [ "$i" -lt 301 ]; do printf ')'; i=$((i + 1)); done
    printf '\n    return 0\nout\n'
} > "$TMP/excessive-expression-depth.brew"
expect_rejected excessive-expression-depth

cat > "$TMP/initialized-on-all-branches.brew" <<'BREW'
fn main():
    var value: int
    if true:
        value = 21
    else:
        value = 22
    print(value)
    return 0
BREW
"$BREW" build "$TMP/initialized-on-all-branches.brew" -o "$TMP/initialized-on-all-branches"
test "$("$TMP/initialized-on-all-branches")" = "21"
printf 'PASS: definite initialization merges assignments from both branches\n'

cat > "$TMP/boolean-abi.brew" <<'BREW'
fn identity(value: bool) -> bool:
    print(0)
    return value
fn main() -> int:
    if identity(true):
        print(21)
    else:
        print(0)
    return 0
BREW
"$BREW" build "$TMP/boolean-abi.brew" -o "$TMP/boolean-abi"
expected=$(printf '0\n21')
test "$("$TMP/boolean-abi")" = "$expected"
printf 'PASS: x86-64 AST backend normalizes boolean parameters and return values\n'

mkdir -p "$TMP/std"
cat > "$TMP/std/helper.brew" <<'BREW'
fn helper_value() -> int:
    return 21
BREW
cat > "$TMP/imported-module.brew" <<'BREW'
import helper
fn main() -> int:
    return helper_value()
BREW
"$BREW" build "$TMP/imported-module.brew" --stdlib "$TMP/std" -o "$TMP/imported-module"
set +e
"$TMP/imported-module"
import_status=$?
set -e
if [ "$import_status" -ne 21 ]; then
    echo "FAIL: imported module returned '$import_status', expected 21" >&2
    exit 1
fi
printf 'PASS: imported module tokens remain valid through parsing and code generation\n'

cat > "$TMP/unknown-function.brew" <<'BREW'
fn main():
    missing_function()
    return 0
BREW
expect_rejected unknown-function

cat > "$TMP/call-arity.brew" <<'BREW'
fn add(left: int, right: int) -> int:
    return left + right
fn main():
    print(add(1))
    return 0
BREW
expect_rejected call-arity

cat > "$TMP/missing-import.brew" <<'BREW'
import module_that_does_not_exist
fn main():
    return 0
BREW
expect_rejected missing-import

cat > "$TMP/return-both-branches.brew" <<'BREW'
fn choose(flag: bool) -> int:
    if flag
        return 11
    else
        return 22
fn main():
    print(choose(true))
    return 0
BREW
"$BREW" build "$TMP/return-both-branches.brew" -o "$TMP/return-both-branches"
test "$("$TMP/return-both-branches")" = "11"
printf 'PASS: function calls and branch return lowering\n'

cat > "$TMP/short-circuit.brew" <<'BREW'
fn main():
    if false and (1 / 0 == 0)
        print(1)
    if true or (1 / 0 == 0)
        print(2)
    return 0
BREW
"$BREW" build "$TMP/short-circuit.brew" -o "$TMP/short-circuit"
test "$("$TMP/short-circuit")" = "2"
printf 'PASS: logical operators short-circuit at runtime\n'

cat > "$TMP/loop-control.brew" <<'BREW'
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
    print(total)
    return 0
BREW
"$BREW" build "$TMP/loop-control.brew" -o "$TMP/loop-control"
test "$("$TMP/loop-control")" = "16"
printf 'PASS: nested break and continue control flow\n'

cat > "$TMP/array-modes.brew" <<'BREW'
fn main():
    let checked = array(4)
    let unchecked = array_unchecked(4)
    var i = 0
    while i < 4
        checked[i] = i + 1
        unchecked[i] = i + 5
        i = i + 1
    print(checked[3])
    print(unchecked[3])
    return 0
BREW
"$BREW" build "$TMP/array-modes.brew" -o "$TMP/array-modes"
test "$("$TMP/array-modes")" = "4
8"
printf 'PASS: checked and unchecked contiguous u32 arrays\n'

cat > "$TMP/signed-remainder.brew" <<'BREW'
fn main():
    var value = -1000
    var total = 50000
    while value <= 1000
        total = total + (value % 97)
        value = value + 1
    print(total)
    return 0
BREW
"$BREW" build "$TMP/signed-remainder.brew" -o "$TMP/signed-remainder"
test "$("$TMP/signed-remainder")" = "50000"
printf 'PASS: signed constant remainder lowering\n'

cat > "$TMP/proven-array-fill.brew" <<'BREW'
fn main():
    let values = array(4)
    var i = 0
    while i < 4
        values[i] = i
        i = i + 1
    print(values[3])
    return 0
BREW
"$BREW" "$TMP/proven-array-fill.brew" --emit-asm -o "$TMP/proven-array-fill.s"
cc -x assembler "$TMP/proven-array-fill.s" -o "$TMP/proven-array-fill"
test "$("$TMP/proven-array-fill")" = "3"
if grep -q 'cmp rcx, 4' "$TMP/proven-array-fill.s"; then
    echo 'FAIL: proven array-fill loop retained a bounds check' >&2
    exit 1
fi
printf 'PASS: proven sequential array-fill loop removes redundant bounds checks\n'
if ! grep -Fq 'mov DWORD PTR [rbx + rcx*4], r12d' "$TMP/proven-array-fill.s"; then
    echo 'FAIL: simple array fill did not lower to a direct register store' >&2
    cat "$TMP/proven-array-fill.s" >&2
    exit 1
fi
if grep -q 'push rax' "$TMP/proven-array-fill.s"; then
    echo 'FAIL: simple array fill retained an unnecessary stack temporary' >&2
    exit 1
fi
printf 'PASS: direct register-to-array stores avoid stack temporaries\n'

cat > "$TMP/samc-auto.brew" <<'BREW'
fn main():
    let values = array(8)
    values[0] = 42
    return 0
BREW
"$BREW" "$TMP/samc-auto.brew" --emit-asm -o "$TMP/samc-auto.s"
if grep -q 'call calloc@PLT' "$TMP/samc-auto.s" || grep -q 'call free@PLT' "$TMP/samc-auto.s"; then
    echo 'FAIL: a small local array used the general-purpose heap allocator' >&2
    exit 1
fi
grep -q 'rep stosd' "$TMP/samc-auto.s"
printf 'PASS: SAMC places small non-escaping arrays in the native stack frame\n'

cat > "$TMP/array-conditional-drop.brew" <<'BREW'
fn main():
    var length = 8
    if false:
        let values = array(length)
    return 0
BREW
"$BREW" build "$TMP/array-conditional-drop.brew" -o "$TMP/array-conditional-drop"
"$TMP/array-conditional-drop"
printf 'PASS: cleanup safely skips arrays whose declaration branch did not execute\n'

cat > "$TMP/array-last-use.brew" <<'BREW'
fn main():
    var length = 8
    let values = array(length)
    values[0] = 5
    print(values[0])
    print(7)
    return 0
BREW
"$BREW" build "$TMP/array-last-use.brew" -o "$TMP/array-last-use"
test "$("$TMP/array-last-use")" = "5
7"
"$BREW" "$TMP/array-last-use.brew" --emit-asm -o "$TMP/array-last-use.s"
first_log=$(grep -n 'call printf@PLT' "$TMP/array-last-use.s" | sed -n '1s/:.*//p')
last_use_drop=$(grep -n 'call free@PLT' "$TMP/array-last-use.s" | sed -n '1s/:.*//p')
second_log=$(grep -n 'call printf@PLT' "$TMP/array-last-use.s" | sed -n '2s/:.*//p')
if [ -z "$first_log" ] || [ -z "$last_use_drop" ] || [ -z "$second_log" ] ||
   [ "$last_use_drop" -le "$first_log" ] || [ "$last_use_drop" -ge "$second_log" ]; then
    echo 'FAIL: owned array was not released immediately after its final use' >&2
    exit 1
fi
printf 'PASS: heap arrays are released immediately after their final use\n'

cat > "$TMP/array-early-return.brew" <<'BREW'
fn main():
    var length = 8
    if true:
        let values = array(length)
        return 0
    return 0
BREW
"$BREW" build "$TMP/array-early-return.brew" -o "$TMP/array-early-return"
"$TMP/array-early-return"
printf 'PASS: early-return cleanup is safe and idempotent\n'

cat > "$TMP/samc-escape.brew" <<'BREW'
fn consume(value):
    print(value)
    return 0
fn main():
    let values = array(8)
    consume(values)
    return 0
BREW
"$BREW" "$TMP/samc-escape.brew" --emit-asm -o "$TMP/samc-escape.s"
if grep -q 'call free@PLT' "$TMP/samc-escape.s"; then
    echo 'FAIL: SAMC freed an array passed to another function' >&2
    exit 1
fi
printf 'PASS: escaped arrays are not prematurely released\n'

printf 'PASS: compiler regression suite\n'

cat > "$TMP/removed-manual-modifier.brew" <<'BREW'
func main():
    manual var values = array(4)
    return 0
BREW
expect_rejected removed-manual-modifier

cat > "$TMP/manual-free.brew" <<'BREW'
func main():
    let values = array(4)
    free(values)
    return 0
BREW
expect_rejected manual-free

