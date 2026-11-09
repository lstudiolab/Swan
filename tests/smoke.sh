#!/usr/bin/env sh
set -eu
# Exercise the low-latency direct x86-64 backend explicitly.
export SWAN_FAST_COMPILE=1
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/test"
mkdir -p "$TMP"

cat > "$TMP/hello.swan" <<'SWAN'
fn main():
    log "Hello"
    return 0
SWAN
"$SWAN" "$TMP/hello.swan" --emit-asm -o "$TMP/hello.s"
${CC:-cc} -x assembler "$TMP/hello.s" -o "$TMP/hello"
test "$("$TMP/hello")" = "Hello"
printf 'PASS: indentation-based hello program\n'

# Swan's current spelling uses print as a built-in and out as the block closer.
cat > "$TMP/explicit-out.swan" <<'SWAN'
fn main():
    let message = 42
    print message
out
SWAN
"$SWAN" "$TMP/explicit-out.swan" --emit-asm -o "$TMP/explicit-out.s"
${CC:-cc} -x assembler "$TMP/explicit-out.s" -o "$TMP/explicit-out"
test "$("$TMP/explicit-out")" = "42"
printf 'PASS: print built-in and explicit out block terminator\n'

# Legacy words are ordinary identifiers, not reserved keywords.
cat > "$TMP/legacy-identifiers.swan" <<'SWAN'
fn main():
    let log = 1
    let manual = 2
    let none = 3
    let extern = 4
    let func = 5
    let and = 6
    let or = 7
    let not = 8
    print log + manual + none + extern + func + and + or + not
out
SWAN
"$SWAN" "$TMP/legacy-identifiers.swan" --emit-asm -o "$TMP/legacy-identifiers.s"
${CC:-cc} -x assembler "$TMP/legacy-identifiers.s" -o "$TMP/legacy-identifiers"
test "$("$TMP/legacy-identifiers")" = "36"
printf 'PASS: removed legacy keywords are valid identifiers\n'


cat > "$TMP/control.swan" <<'SWAN'
fn main():
    var x: int = 0
    while x < 3
        if x == 1
            log x
        else
            log 0
        x = x + 1
    return 0
SWAN
"$SWAN" "$TMP/control.swan" --emit-asm -o "$TMP/control.s"
${CC:-cc} -x assembler "$TMP/control.s" -o "$TMP/control"
test "$("$TMP/control")" = "0
1
0"
printf 'PASS: nested indentation-based control flow\n'

cat > "$TMP/multi-dedent.swan" <<'SWAN'
fn main():
    if true
        while false
            if true
                log "unreachable"
        log "inner blocks closed together"
    log "outer block closed"
    return 0
SWAN
"$SWAN" "$TMP/multi-dedent.swan" --emit-asm -o "$TMP/multi-dedent.s"
${CC:-cc} -x assembler "$TMP/multi-dedent.s" -o "$TMP/multi-dedent"
test "$("$TMP/multi-dedent")" = "inner blocks closed together
outer block closed"
printf 'PASS: one dedent closes multiple nested blocks\n'

cat > "$TMP/functions.swan" <<'SWAN'
fn answer():
    return 42
fn main():
    log answer()
    return 0
SWAN
"$SWAN" "$TMP/functions.swan" --emit-asm -o "$TMP/functions.s"
${CC:-cc} -x assembler "$TMP/functions.s" -o "$TMP/functions"
test "$("$TMP/functions")" = "42"
printf 'PASS: inferred integer return type and inline helper emission\n'

cat > "$TMP/int64.swan" <<'SWAN'
fn main():
    let large: int = 3000000000
    log large
    return 0
SWAN
"$SWAN" "$TMP/int64.swan" --emit-asm -o "$TMP/int64.s"
${CC:-cc} -x assembler "$TMP/int64.s" -o "$TMP/int64"
test "$("$TMP/int64")" = "3000000000"
printf 'PASS: 64-bit integer lowering\n'

# Struct declarations are not part of the active C11 subset yet.

cat > "$TMP/invalid.swan" <<'SWAN'
fn main():
    log missing
    return 0
SWAN
if "$SWAN" "$TMP/invalid.swan" --emit-asm -o "$TMP/invalid.s" 2>/dev/null; then
  echo 'FAIL: semantic error accepted' >&2
  exit 1
fi
printf 'PASS: semantic errors rejected\n'

cat > "$TMP/identifier-location.swan" <<'SWAN'
fn main():
    log missing
    return 0
SWAN
if "$SWAN" "$TMP/identifier-location.swan" --emit-asm -o "$TMP/identifier-location.s" 2>"$TMP/identifier-location.err"; then
  echo 'FAIL: unknown identifier accepted' >&2
  exit 1
fi
grep -q "2:9: unknown name 'missing'" "$TMP/identifier-location.err"
printf 'PASS: identifier source columns\n'

cat > "$TMP/keyword-operators.swan" <<'SWAN'
fn main():
    var enabled: bool = true
    if enabled and not false or false
        log "keyword operators preserved"
    return 0
SWAN
"$SWAN" "$TMP/keyword-operators.swan" --emit-asm -o "$TMP/keyword-operators.s"
${CC:-cc} -x assembler "$TMP/keyword-operators.s" -o "$TMP/keyword-operators"
test "$("$TMP/keyword-operators")" = "keyword operators preserved"
printf 'PASS: keyword operators and primitive types\n'

# Tabs count as four indentation columns while occupying one source column.
printf 'fn main():\n\tlog "tab indentation"\n\treturn 0\n' > "$TMP/tabs.swan"
"$SWAN" "$TMP/tabs.swan" --emit-asm -o "$TMP/tabs.s"
${CC:-cc} -x assembler "$TMP/tabs.s" -o "$TMP/tabs"
test "$("$TMP/tabs")" = "tab indentation"
printf 'PASS: tab indentation width and source scanning\n'

# A dedent must match an existing indentation level exactly.
printf 'fn main():\n    if true\n        log "nested"\n      log "invalid"\n' > "$TMP/bad-indent.swan"
if "$SWAN" "$TMP/bad-indent.swan" --emit-asm -o "$TMP/bad-indent.s" 2>/dev/null; then
  echo 'FAIL: inconsistent indentation accepted' >&2
  exit 1
fi
printf 'PASS: inconsistent indentation rejected\n'

cat > "$TMP/string-escapes.swan" <<'SWAN'
fn main():
    log "quote: \" slash: \\ newline:\n tab:\t"
    return 0
SWAN
"$SWAN" "$TMP/string-escapes.swan" --emit-asm -o "$TMP/string-escapes.s"
${CC:-cc} -x assembler "$TMP/string-escapes.s" -o "$TMP/string-escapes"
expected=$(printf 'quote: " slash: \\ newline:\n tab:\t')
test "$("$TMP/string-escapes")" = "$expected"
printf 'PASS: native string escaping preserves special characters\n'

# The standard source-file invocation compiles a native executable and runs it.
test "$("$SWAN" "$ROOT/examples/hello.swan")" = "Hello from Swan"
printf 'PASS: direct .swan compile-and-run\n'

# Build creates a reusable native executable without running it.
"$SWAN" build "$ROOT/examples/hello.swan" -o "$TMP/hello-native"
test -x "$TMP/hello-native"
test "$("$TMP/hello-native")" = "Hello from Swan"
printf 'PASS: native executable build\n'

# Explicit run has the same behavior as the shorthand invocation.
test "$("$SWAN" run "$ROOT/examples/hello.swan")" = "Hello from Swan"
printf 'PASS: explicit run command\n'

# An explicitly named run output remains available after execution.
"$SWAN" run "$ROOT/examples/hello.swan" -o "$TMP/hello-run"
test -x "$TMP/hello-run"
test "$("$TMP/hello-run")" = "Hello from Swan"
printf 'PASS: named run executable is preserved\n'

# Heap arrays support allocation, indexed writes, indexed reads, and mutation.
cat > "$TMP/arrays.swan" <<'SWAN'
fn main():
    let values = array(8)
    var i = 0
    while i < 8
        values[i] = i * 3
        i = i + 1
    var sum = 0
    i = 0
    while i < 8
        log values[i]
        sum = sum + values[i]
        i = i + 1
    log sum
    return 0
SWAN
"$SWAN" "$TMP/arrays.swan" --emit-asm -o "$TMP/arrays.s"
${CC:-cc} -x assembler "$TMP/arrays.s" -o "$TMP/arrays"
actual=$( "$TMP/arrays" )
expected=$(printf '0\n3\n6\n9\n12\n15\n18\n21\n84')
if [ "$actual" != "$expected" ]; then echo "FAIL: array workload expected scaled sequence and sum, got [$actual]" >&2; exit 1; fi
printf 'PASS: heap integer arrays, indexed writes, and indexed reads\n'

# Constant-sized checked arrays keep their length in compiler metadata.
# Dynamic indices compare against an immediate; in-range constant indices use
# a direct displacement and need neither an index register nor a bounds branch.
cat > "$TMP/array-known-length.swan" <<'SWAN'
fn main():
    let values = array(8)
    values[3] = 41
    var index = 2
    values[index] = 17
    log values[3]
    log values[index]
    return 0
SWAN
"$SWAN" "$TMP/array-known-length.swan" --emit-asm -o "$TMP/array-known-length.s"
grep -Eq 'cmp rcx, 8' "$TMP/array-known-length.s"
grep -Eq 'DWORD PTR \[[a-z0-9]+ \+ 12\]' "$TMP/array-known-length.s"
grep -Eq 'mov edi, 10' "$TMP/array-known-length.s"
if grep -q 'add rax, 2' "$TMP/array-known-length.s"; then
  echo 'FAIL: constant array allocation retained dynamic length arithmetic' >&2
  exit 1
fi
${CC:-cc} -x assembler "$TMP/array-known-length.s" -o "$TMP/array-known-length"
test "$("$TMP/array-known-length")" = "41
17"
printf 'PASS: fixed-length array bounds and constant-index fast paths\\n'


# Nested calls and calls inside expressions must preserve stack alignment.
cat > "$TMP/nested-calls.swan" <<'SWAN'
fn value() -> int:
    return 7
fn add(a: int, b: int) -> int:
    return a + b
fn main():
    log 5 + value()
    log add(1, add(2, 3))
    return 0
SWAN
"$SWAN" "$TMP/nested-calls.swan" --emit-asm -o "$TMP/nested-calls.s"
${CC:-cc} -x assembler "$TMP/nested-calls.s" -o "$TMP/nested-calls"
test "$("$TMP/nested-calls")" = "12
6"
printf 'PASS: nested calls and ABI stack alignment\\n'

# Power-of-two integer division preserves signed truncation toward zero.
cat > "$TMP/power-two-division.swan" <<'SWAN'
fn main():
    log 3 // 2
    log -3 // 2
    log -1 // 2
    log -5 // 4
    log 5 // 4
    return 0
SWAN
"$SWAN" "$TMP/power-two-division.swan" --emit-asm -o "$TMP/power-two-division.s"
${CC:-cc} -x assembler "$TMP/power-two-division.s" -o "$TMP/power-two-division"
test "$("$TMP/power-two-division")" = "1
-1
0
-1
1"
printf 'PASS: signed power-of-two integer division\n'

# Caller-saved registers used for locals survive calls and output operations.
cat > "$TMP/caller-saved-locals.swan" <<'SWAN'
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
    log answer()
    log f + g + h + i
    log modulo
    log negativeModulo
    log smallModulo
    log positiveModulo
    return 0
SWAN
"$SWAN" "$TMP/caller-saved-locals.swan" --emit-asm -o "$TMP/caller-saved-locals.s"
${CC:-cc} -x assembler "$TMP/caller-saved-locals.s" -o "$TMP/caller-saved-locals"
test "$("$TMP/caller-saved-locals")" = "10
30
0
-1
-2
2"
printf 'PASS: caller-saved local registers survive calls\n'

# Binary search exercises allocation, indexed memory, arithmetic, and nested control flow.
cat > "$TMP/binary-search.swan" <<'SWAN'
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
    log checksum
    return 0
SWAN
"$SWAN" "$TMP/binary-search.swan" --emit-asm -o "$TMP/binary-search.s"
${CC:-cc} -x assembler "$TMP/binary-search.s" -o "$TMP/binary-search"
test "$("$TMP/binary-search")" = "120"
printf 'PASS: binary search over heap arrays\\n'

# The full 20M-element / 20M-query benchmark must compile without executing in CI.
"$SWAN" build "$ROOT/benchmarks/binary_search.swan" -o "$TMP/binary-search-20m"
test -x "$TMP/binary-search-20m"
printf 'PASS: full 20M x 20M binary-search workload compiles\n'
