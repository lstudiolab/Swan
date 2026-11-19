#!/usr/bin/env sh
set -eu
# Exercise the low-latency direct x86-64 backend explicitly.
export BREW_FAST_COMPILE=1
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brew"
TMP="$ROOT/build/test"
mkdir -p "$TMP"

cat > "$TMP/hello.brew" <<'BREW'
fn main():
    print("Hello")
    return 0
BREW
"$BREW" "$TMP/hello.brew" --emit-asm -o "$TMP/hello.s"
${CC:-cc} -x assembler "$TMP/hello.s" -o "$TMP/hello"
test "$("$TMP/hello")" = "Hello"
printf 'PASS: indentation-based hello program\n'

# Blocks are closed only by dedentation; no terminator token is used.
cat > "$TMP/indentation-block.brew" <<'BREW'
func main() -> int:
    let message = 42
    if true:
        print(message)
    return 0
BREW
"$BREW" "$TMP/indentation-block.brew" --emit-asm -o "$TMP/indentation-block.s"
${CC:-cc} -x assembler "$TMP/indentation-block.s" -o "$TMP/indentation-block"
test "$("$TMP/indentation-block")" = "42"
printf 'PASS: indentation-only block termination\\n'

# Legacy words are ordinary identifiers, not reserved keywords.
cat > "$TMP/legacy-identifiers.brew" <<'BREW'
fn main():
    let log = 1
    let manualCount = 2
    let none = 3
    let extern = 4
    let funcName = 5
    let and = 6
    let or = 7
    let not = 8
    print(log + manualCount + none + extern + funcName)
BREW
"$BREW" "$TMP/legacy-identifiers.brew" --emit-asm -o "$TMP/legacy-identifiers.s"
${CC:-cc} -x assembler "$TMP/legacy-identifiers.s" -o "$TMP/legacy-identifiers"
test "$("$TMP/legacy-identifiers")" = "15"
printf 'PASS: removed legacy keywords are valid identifiers\n'


cat > "$TMP/control.brew" <<'BREW'
fn main():
    var x: int = 0
    while x < 3
        if x == 1
            print(x)
        else
            print(0)
        x = x + 1
    return 0
BREW
"$BREW" "$TMP/control.brew" --emit-asm -o "$TMP/control.s"
${CC:-cc} -x assembler "$TMP/control.s" -o "$TMP/control"
test "$("$TMP/control")" = "0
1
0"
printf 'PASS: nested indentation-based control flow\n'

cat > "$TMP/multi-dedent.brew" <<'BREW'
fn main():
    if true
        while false
            if true
                print("unreachable")
        print("inner blocks closed together")
    print("outer block closed")
    return 0
BREW
"$BREW" "$TMP/multi-dedent.brew" --emit-asm -o "$TMP/multi-dedent.s"
${CC:-cc} -x assembler "$TMP/multi-dedent.s" -o "$TMP/multi-dedent"
test "$("$TMP/multi-dedent")" = "inner blocks closed together
outer block closed"
printf 'PASS: one dedent closes multiple nested blocks\n'

cat > "$TMP/functions.brew" <<'BREW'
fn answer():
    return 42
fn main():
    print(answer())
    return 0
BREW
"$BREW" "$TMP/functions.brew" --emit-asm -o "$TMP/functions.s"
${CC:-cc} -x assembler "$TMP/functions.s" -o "$TMP/functions"
test "$("$TMP/functions")" = "42"
printf 'PASS: inferred integer return type and inline helper emission\n'

cat > "$TMP/int64.brew" <<'BREW'
fn main():
    let large: int = 3000000000
    print(large)
    return 0
BREW
"$BREW" "$TMP/int64.brew" --emit-asm -o "$TMP/int64.s"
${CC:-cc} -x assembler "$TMP/int64.s" -o "$TMP/int64"
test "$("$TMP/int64")" = "3000000000"
printf 'PASS: 64-bit integer lowering\n'


# "manual" is an ordinary identifier, not a memory-management keyword.
cat > "$TMP/manual-identifier.brew" <<'BREW'
fn main():
    let manual = 27
    print(manual)
    return 0
BREW
"$BREW" "$TMP/manual-identifier.brew" --emit-asm -o "$TMP/manual-identifier.s"
${CC:-cc} -x assembler "$TMP/manual-identifier.s" -o "$TMP/manual-identifier"
test "$("$TMP/manual-identifier")" = "27"
printf 'PASS: manual is an ordinary identifier, not a keyword\\n'

# The removed manual modifier is rejected rather than silently changing ownership.
cat > "$TMP/manual-modifier.brew" <<'BREW'
fn main():
    manual var values = array(4)
    return 0
BREW
if "$BREW" "$TMP/manual-modifier.brew" --emit-asm -o "$TMP/manual-modifier.s" 2>/dev/null; then
  echo 'FAIL: removed manual modifier was accepted' >&2
  exit 1
fi
printf 'PASS: removed manual memory modifier is rejected\\n'

# Struct declarations are not part of the active C11 subset yet.

cat > "$TMP/invalid.brew" <<'BREW'
fn main():
    print(missing)
    return 0
BREW
if "$BREW" "$TMP/invalid.brew" --emit-asm -o "$TMP/invalid.s" 2>/dev/null; then
  echo 'FAIL: semantic error accepted' >&2
  exit 1
fi
printf 'PASS: semantic errors rejected\n'

cat > "$TMP/identifier-location.brew" <<'BREW'
fn main():
    print(missing)
    return 0
BREW
if "$BREW" "$TMP/identifier-location.brew" --emit-asm -o "$TMP/identifier-location.s" 2>"$TMP/identifier-location.err"; then
  echo 'FAIL: unknown identifier accepted' >&2
  exit 1
fi
grep -q "2:11: unknown name 'missing'" "$TMP/identifier-location.err"
printf 'PASS: identifier source columns\n'

cat > "$TMP/keyword-operators.brew" <<'BREW'
fn main():
    var enabled: bool = true
    if enabled and not false or false
        print("keyword operators preserved")
    return 0
BREW
"$BREW" "$TMP/keyword-operators.brew" --emit-asm -o "$TMP/keyword-operators.s"
${CC:-cc} -x assembler "$TMP/keyword-operators.s" -o "$TMP/keyword-operators"
test "$("$TMP/keyword-operators")" = "keyword operators preserved"
printf 'PASS: keyword operators and primitive types\n'

# Tabs count as four indentation columns while occupying one source column.
printf 'fn main():\n\tprint("tab indentation")\n\treturn 0\n' > "$TMP/tabs.brew"
"$BREW" "$TMP/tabs.brew" --emit-asm -o "$TMP/tabs.s"
${CC:-cc} -x assembler "$TMP/tabs.s" -o "$TMP/tabs"
test "$("$TMP/tabs")" = "tab indentation"
printf 'PASS: tab indentation width and source scanning\n'

# A dedent must match an existing indentation level exactly.
printf 'fn main():\n    if true\n        print("nested")\n      print("invalid")\n' > "$TMP/bad-indent.brew"
if "$BREW" "$TMP/bad-indent.brew" --emit-asm -o "$TMP/bad-indent.s" 2>/dev/null; then
  echo 'FAIL: inconsistent indentation accepted' >&2
  exit 1
fi
printf 'PASS: inconsistent indentation rejected\n'

cat > "$TMP/string-escapes.brew" <<'BREW'
fn main():
    print("quote: \" slash: \\ newline:\n tab:\t")
    return 0
BREW
"$BREW" "$TMP/string-escapes.brew" --emit-asm -o "$TMP/string-escapes.s"
${CC:-cc} -x assembler "$TMP/string-escapes.s" -o "$TMP/string-escapes"
expected=$(printf 'quote: " slash: \\ newline:\n tab:\t')
test "$("$TMP/string-escapes")" = "$expected"
printf 'PASS: native string escaping preserves special characters\n'

# The standard source-file invocation compiles a native executable and runs it.
test "$("$BREW" "$ROOT/examples/hello.brew")" = "Hello from Brew"
printf 'PASS: direct .brew compile-and-run\n'

# Build creates a reusable native executable without running it.
"$BREW" build "$ROOT/examples/hello.brew" -o "$TMP/hello-native"
test -x "$TMP/hello-native"
test "$("$TMP/hello-native")" = "Hello from Brew"
printf 'PASS: native executable build\n'

# Explicit run has the same behavior as the shorthand invocation.
test "$("$BREW" run "$ROOT/examples/hello.brew")" = "Hello from Brew"
printf 'PASS: explicit run command\n'

# An explicitly named run output remains available after execution.
"$BREW" run "$ROOT/examples/hello.brew" -o "$TMP/hello-run"
test -x "$TMP/hello-run"
test "$("$TMP/hello-run")" = "Hello from Brew"
printf 'PASS: named run executable is preserved\n'

# Heap arrays support allocation, indexed writes, indexed reads, and mutation.
cat > "$TMP/arrays.brew" <<'BREW'
fn main():
    let values = array(8)
    var i = 0
    while i < 8
        values[i] = i * 3
        i = i + 1
    var sum = 0
    i = 0
    while i < 8
        print(values[i])
        sum = sum + values[i]
        i = i + 1
    print(sum)
    return 0
BREW
"$BREW" "$TMP/arrays.brew" --emit-asm -o "$TMP/arrays.s"
${CC:-cc} -x assembler "$TMP/arrays.s" -o "$TMP/arrays"
actual=$( "$TMP/arrays" )
expected=$(printf '0\n3\n6\n9\n12\n15\n18\n21\n84')
if [ "$actual" != "$expected" ]; then echo "FAIL: array workload expected scaled sequence and sum, got [$actual]" >&2; exit 1; fi
printf 'PASS: heap integer arrays, indexed writes, and indexed reads\n'

# Constant-sized checked arrays keep their length in compiler metadata.
# Dynamic indices compare against an immediate; in-range constant indices use
# a direct displacement and need neither an index register nor a bounds branch.
cat > "$TMP/array-known-length.brew" <<'BREW'
fn main():
    let values = array(8)
    values[3] = 41
    var index = 2
    values[index] = 17
    print(values[3])
    print(values[index])
    return 0
BREW
"$BREW" "$TMP/array-known-length.brew" --emit-asm -o "$TMP/array-known-length.s"
grep -Eq 'cmp rcx, 8' "$TMP/array-known-length.s"
grep -Eq 'DWORD PTR \[[a-z0-9]+ \+ 12\]' "$TMP/array-known-length.s"
grep -q 'rep stosd' "$TMP/array-known-length.s"
if grep -q 'call calloc@PLT' "$TMP/array-known-length.s"; then
  echo 'FAIL: fixed non-escaping array used heap allocation' >&2
  exit 1
fi
if grep -q 'add rax, 2' "$TMP/array-known-length.s"; then
  echo 'FAIL: constant array allocation retained dynamic length arithmetic' >&2
  exit 1
fi
${CC:-cc} -x assembler "$TMP/array-known-length.s" -o "$TMP/array-known-length"
test "$("$TMP/array-known-length")" = "41
17"
printf 'PASS: fixed-length array bounds and constant-index fast paths\\n'


# Nested calls and calls inside expressions must preserve stack alignment.
cat > "$TMP/nested-calls.brew" <<'BREW'
fn value() -> int:
    return 7
fn add(a: int, b: int) -> int:
    return a + b
fn main():
    print(5 + value())
    print(add(1, add(2, 3)))
    return 0
BREW
"$BREW" "$TMP/nested-calls.brew" --emit-asm -o "$TMP/nested-calls.s"
${CC:-cc} -x assembler "$TMP/nested-calls.s" -o "$TMP/nested-calls"
test "$("$TMP/nested-calls")" = "12
6"
printf 'PASS: nested calls and ABI stack alignment\\n'

# Power-of-two integer division preserves signed truncation toward zero.
cat > "$TMP/power-two-division.brew" <<'BREW'
fn main():
    print(3 // 2)
    print(-3 // 2)
    print(-1 // 2)
    print(-5 // 4)
    print(5 // 4)
    return 0
BREW
"$BREW" "$TMP/power-two-division.brew" --emit-asm -o "$TMP/power-two-division.s"
${CC:-cc} -x assembler "$TMP/power-two-division.s" -o "$TMP/power-two-division"
test "$("$TMP/power-two-division")" = "1
-1
0
-1
1"
printf 'PASS: signed power-of-two integer division\n'

# Caller-saved registers used for locals survive calls and output operations.
cat > "$TMP/caller-saved-locals.brew" <<'BREW'
fn answer() -> int:
    return 10
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
    let modulo = 194 % 97
    let negativeModulo = -195 % 97
    let smallModulo = -5 % 3
    let positiveModulo = 5 % 3
    print(answer())
    print(f + g + h + i)
    print(modulo)
    print(negativeModulo)
    print(smallModulo)
    print(positiveModulo)
    return 0
BREW
"$BREW" "$TMP/caller-saved-locals.brew" --emit-asm -o "$TMP/caller-saved-locals.s"
${CC:-cc} -x assembler "$TMP/caller-saved-locals.s" -o "$TMP/caller-saved-locals"
test "$("$TMP/caller-saved-locals")" = "10
30
0
-1
-2
2"
printf 'PASS: caller-saved local registers survive calls\n'

# Binary search exercises allocation, indexed memory, arithmetic, and nested control flow.
cat > "$TMP/binary-search.brew" <<'BREW'
fn main():
    let values = array(16)
    var i = 0
    while i < 16
        values[i] = i
        i = i + 1
    var query = 0
    var checksum = 0
    while query < 16
        var target = (query * 7) % 16
        var low = 0
        var high = 15
        var found = -1
        while low <= high
            var middle = low + (high - low) // 2
            var value = values[middle]
            if value == target
                found = value
                break
            else
                if value < target
                    low = middle + 1
                else
                    high = middle - 1
        checksum = checksum + found
        query = query + 1
    print(checksum)
    return 0
BREW
"$BREW" "$TMP/binary-search.brew" --emit-asm -o "$TMP/binary-search.s"
${CC:-cc} -x assembler "$TMP/binary-search.s" -o "$TMP/binary-search"
test "$("$TMP/binary-search")" = "120"
printf 'PASS: binary search over heap arrays\\n'

# The full 20M-element / 20M-query benchmark must compile without executing in CI.
"$BREW" build "$ROOT/benchmarks/binary_search.brew" -o "$TMP/binary-search-20m"
test -x "$TMP/binary-search-20m"
printf 'PASS: full 20M x 20M binary-search workload compiles\n'

