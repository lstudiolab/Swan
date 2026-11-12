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

# Explicit ':' / 'out' blocks are delimiter-based, not indentation-based.
cat > "$TMP/explicit-out.swan" <<'SWAN'
fn main():
    let message = 42
    if true:
        print message
    out
    return 0
out
SWAN
"$SWAN" "$TMP/explicit-out.swan" --emit-asm -o "$TMP/explicit-out.s"
${CC:-cc} -x assembler "$TMP/explicit-out.s" -o "$TMP/explicit-out"
test "$("$TMP/explicit-out")" = "42"
printf 'PASS: explicit colon/out blocks\\n'


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
    let manualCount = 2
    let none = 3
    let extern = 4
    let funcName = 5
    let and = 6
    let or = 7
    let not = 8
    print log + manualCount + none + extern + funcName
out
SWAN
"$SWAN" "$TMP/legacy-identifiers.swan" --emit-asm -o "$TMP/legacy-identifiers.s"
${CC:-cc} -x assembler "$TMP/legacy-identifiers.s" -o "$TMP/legacy-identifiers"
test "$("$TMP/legacy-identifiers")" = "15"
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


# "manual" is an ordinary identifier, not a memory-management keyword.
cat > "$TMP/manual-identifier.swan" <<'SWAN'
fn main():
    let manual = 27
    print manual
    return 0
out
SWAN
"$SWAN" "$TMP/manual-identifier.swan" --emit-asm -o "$TMP/manual-identifier.s"
${CC:-cc} -x assembler "$TMP/manual-identifier.s" -o "$TMP/manual-identifier"
test "$("$TMP/manual-identifier")" = "27"
printf 'PASS: manual is an ordinary identifier, not a keyword\\n'

# The removed manual modifier is rejected rather than silently changing ownership.
cat > "$TMP/manual-modifier.swan" <<'SWAN'
fn main():
    manual var values = array(4)
    return 0
out
SWAN
if "$SWAN" "$TMP/manual-modifier.swan" --emit-asm -o "$TMP/manual-modifier.s" 2>/dev/null; then
  echo 'FAIL: removed manual modifier was accepted' >&2
  exit 1
fi
printf 'PASS: removed manual memory modifier is rejected\\n'

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

# Compile and execute Compiler 2's Swan-authored lexer through Compiler 1.
# The temporary main is a test harness; production Lexer.swan remains a module.
cat "$ROOT/swanc/Parse/Lexer.swan" > "$TMP/swanc-lexer-test.swan"
cat >> "$TMP/swanc-lexer-test.swan" <<'SWAN'
fn main() -> int:
    let word = array_unchecked(2)
    word[0] = 105
    word[1] = 102
    if swanc_utf8_width(word, 0, 2) != 1
        return 1
    if swanc_keyword(word, 0, 2) != 15
        return 2
    let prefix = array_unchecked(4)
    prefix[0] = 105
    prefix[1] = 102
    prefix[2] = 102
    prefix[3] = 121
    if swanc_keyword(prefix, 0, 4) != 1
        return 3
    let keyword_source = array_unchecked(247)
    keyword_source[0] = 105
    keyword_source[1] = 109
    keyword_source[2] = 112
    keyword_source[3] = 111
    keyword_source[4] = 114
    keyword_source[5] = 116
    keyword_source[6] = 32
    keyword_source[7] = 110
    keyword_source[8] = 97
    keyword_source[9] = 109
    keyword_source[10] = 101
    keyword_source[11] = 115
    keyword_source[12] = 112
    keyword_source[13] = 97
    keyword_source[14] = 99
    keyword_source[15] = 101
    keyword_source[16] = 32
    keyword_source[17] = 101
    keyword_source[18] = 120
    keyword_source[19] = 112
    keyword_source[20] = 111
    keyword_source[21] = 114
    keyword_source[22] = 116
    keyword_source[23] = 32
    keyword_source[24] = 115
    keyword_source[25] = 116
    keyword_source[26] = 114
    keyword_source[27] = 117
    keyword_source[28] = 99
    keyword_source[29] = 116
    keyword_source[30] = 32
    keyword_source[31] = 101
    keyword_source[32] = 110
    keyword_source[33] = 117
    keyword_source[34] = 109
    keyword_source[35] = 32
    keyword_source[36] = 116
    keyword_source[37] = 121
    keyword_source[38] = 112
    keyword_source[39] = 101
    keyword_source[40] = 32
    keyword_source[41] = 98
    keyword_source[42] = 111
    keyword_source[43] = 111
    keyword_source[44] = 108
    keyword_source[45] = 32
    keyword_source[46] = 118
    keyword_source[47] = 97
    keyword_source[48] = 114
    keyword_source[49] = 105
    keyword_source[50] = 97
    keyword_source[51] = 110
    keyword_source[52] = 116
    keyword_source[53] = 32
    keyword_source[54] = 102
    keyword_source[55] = 117
    keyword_source[56] = 110
    keyword_source[57] = 99
    keyword_source[58] = 32
    keyword_source[59] = 116
    keyword_source[60] = 104
    keyword_source[61] = 114
    keyword_source[62] = 101
    keyword_source[63] = 97
    keyword_source[64] = 100
    keyword_source[65] = 32
    keyword_source[66] = 108
    keyword_source[67] = 101
    keyword_source[68] = 116
    keyword_source[69] = 32
    keyword_source[70] = 118
    keyword_source[71] = 97
    keyword_source[72] = 114
    keyword_source[73] = 32
    keyword_source[74] = 99
    keyword_source[75] = 111
    keyword_source[76] = 110
    keyword_source[77] = 115
    keyword_source[78] = 116
    keyword_source[79] = 32
    keyword_source[80] = 101
    keyword_source[81] = 120
    keyword_source[82] = 116
    keyword_source[83] = 101
    keyword_source[84] = 114
    keyword_source[85] = 110
    keyword_source[86] = 97
    keyword_source[87] = 108
    keyword_source[88] = 32
    keyword_source[89] = 105
    keyword_source[90] = 102
    keyword_source[91] = 32
    keyword_source[92] = 101
    keyword_source[93] = 108
    keyword_source[94] = 115
    keyword_source[95] = 101
    keyword_source[96] = 32
    keyword_source[97] = 119
    keyword_source[98] = 104
    keyword_source[99] = 105
    keyword_source[100] = 108
    keyword_source[101] = 101
    keyword_source[102] = 32
    keyword_source[103] = 102
    keyword_source[104] = 111
    keyword_source[105] = 114
    keyword_source[106] = 32
    keyword_source[107] = 105
    keyword_source[108] = 110
    keyword_source[109] = 32
    keyword_source[110] = 108
    keyword_source[111] = 111
    keyword_source[112] = 111
    keyword_source[113] = 112
    keyword_source[114] = 32
    keyword_source[115] = 98
    keyword_source[116] = 108
    keyword_source[117] = 111
    keyword_source[118] = 99
    keyword_source[119] = 107
    keyword_source[120] = 32
    keyword_source[121] = 103
    keyword_source[122] = 117
    keyword_source[123] = 97
    keyword_source[124] = 114
    keyword_source[125] = 100
    keyword_source[126] = 32
    keyword_source[127] = 100
    keyword_source[128] = 101
    keyword_source[129] = 102
    keyword_source[130] = 101
    keyword_source[131] = 114
    keyword_source[132] = 32
    keyword_source[133] = 109
    keyword_source[134] = 97
    keyword_source[135] = 116
    keyword_source[136] = 99
    keyword_source[137] = 104
    keyword_source[138] = 32
    keyword_source[139] = 97
    keyword_source[140] = 115
    keyword_source[141] = 121
    keyword_source[142] = 110
    keyword_source[143] = 99
    keyword_source[144] = 32
    keyword_source[145] = 97
    keyword_source[146] = 115
    keyword_source[147] = 32
    keyword_source[148] = 114
    keyword_source[149] = 101
    keyword_source[150] = 116
    keyword_source[151] = 117
    keyword_source[152] = 114
    keyword_source[153] = 110
    keyword_source[154] = 32
    keyword_source[155] = 98
    keyword_source[156] = 114
    keyword_source[157] = 101
    keyword_source[158] = 97
    keyword_source[159] = 107
    keyword_source[160] = 32
    keyword_source[161] = 99
    keyword_source[162] = 111
    keyword_source[163] = 110
    keyword_source[164] = 116
    keyword_source[165] = 105
    keyword_source[166] = 110
    keyword_source[167] = 117
    keyword_source[168] = 101
    keyword_source[169] = 32
    keyword_source[170] = 116
    keyword_source[171] = 114
    keyword_source[172] = 117
    keyword_source[173] = 101
    keyword_source[174] = 32
    keyword_source[175] = 102
    keyword_source[176] = 97
    keyword_source[177] = 108
    keyword_source[178] = 115
    keyword_source[179] = 101
    keyword_source[180] = 32
    keyword_source[181] = 110
    keyword_source[182] = 117
    keyword_source[183] = 108
    keyword_source[184] = 108
    keyword_source[185] = 32
    keyword_source[186] = 115
    keyword_source[187] = 101
    keyword_source[188] = 108
    keyword_source[189] = 102
    keyword_source[190] = 32
    keyword_source[191] = 110
    keyword_source[192] = 105
    keyword_source[193] = 108
    keyword_source[194] = 32
    keyword_source[195] = 112
    keyword_source[196] = 97
    keyword_source[197] = 115
    keyword_source[198] = 115
    keyword_source[199] = 32
    keyword_source[200] = 108
    keyword_source[201] = 97
    keyword_source[202] = 122
    keyword_source[203] = 121
    keyword_source[204] = 32
    keyword_source[205] = 102
    keyword_source[206] = 114
    keyword_source[207] = 111
    keyword_source[208] = 109
    keyword_source[209] = 32
    keyword_source[210] = 112
    keyword_source[211] = 114
    keyword_source[212] = 111
    keyword_source[213] = 116
    keyword_source[214] = 111
    keyword_source[215] = 99
    keyword_source[216] = 111
    keyword_source[217] = 108
    keyword_source[218] = 32
    keyword_source[219] = 99
    keyword_source[220] = 97
    keyword_source[221] = 115
    keyword_source[222] = 101
    keyword_source[223] = 32
    keyword_source[224] = 115
    keyword_source[225] = 101
    keyword_source[226] = 116
    keyword_source[227] = 32
    keyword_source[228] = 109
    keyword_source[229] = 97
    keyword_source[230] = 110
    keyword_source[231] = 117
    keyword_source[232] = 97
    keyword_source[233] = 108
    keyword_source[234] = 32
    keyword_source[235] = 102
    keyword_source[236] = 110
    keyword_source[237] = 32
    keyword_source[238] = 111
    keyword_source[239] = 117
    keyword_source[240] = 116
    keyword_source[241] = 32
    keyword_source[242] = 115
    keyword_source[243] = 116
    keyword_source[244] = 111
    keyword_source[245] = 114
    keyword_source[246] = 101
    let keyword_tokens = array_unchecked(225)
    let keyword_result = swan_lex(keyword_source, 247, keyword_tokens, 45, diagnostic)
    if keyword_result != 45
        return 20
    var keyword_index = 0
    while keyword_index < 44:
        if keyword_tokens[keyword_index * 5] == 1
            return 21
        keyword_index = keyword_index + 1
    if keyword_tokens[220] != 0
        return 22
    let empty_source = array_unchecked(1)
    let tokens = array_unchecked(10)
    let diagnostic = array_unchecked(4)
    let count = swan_lex(empty_source, 0, tokens, 2, diagnostic)
    if count != 1
        return 4
    if tokens[0] != 0
        return 5
    let input = array_unchecked(3)
    input[0] = 105
    input[1] = 102
    input[2] = 32
    let scanned = swan_lex(input, 3, tokens, 2, diagnostic)
    if scanned != 2
        return 6
    if tokens[0] != 15
        return 7
    if tokens[5] != 0
        return 8
    let program = array_unchecked(24)
    program[0] = 102
    program[1] = 117
    program[2] = 110
    program[3] = 99
    program[4] = 32
    program[5] = 120
    program[6] = 40
    program[7] = 41
    program[8] = 58
    program[9] = 10
    program[10] = 32
    program[11] = 32
    program[12] = 32
    program[13] = 32
    program[14] = 114
    program[15] = 101
    program[16] = 116
    program[17] = 117
    program[18] = 114
    program[19] = 110
    program[20] = 32
    program[21] = 52
    program[22] = 50
    program[23] = 10
    let program_tokens = array_unchecked(60)
    let full_count = swan_lex(program, 24, program_tokens, 12, diagnostic)
    if full_count != 12
        return 9
    if program_tokens[0] != 8 or program_tokens[5] != 1 or program_tokens[10] != 28
        return 10
    if program_tokens[15] != 29 or program_tokens[20] != 30 or program_tokens[25] != 5
        return 11
    if program_tokens[30] != 6 or program_tokens[35] != 14 or program_tokens[40] != 2
        return 12
    if program_tokens[45] != 5 or program_tokens[50] != 7 or program_tokens[55] != 0
        return 13
    if program_tokens[31] != 14 or program_tokens[33] != 2 or program_tokens[34] != 5
        return 14
    let text_input = array_unchecked(9)
    text_input[0] = 108
    text_input[1] = 111
    text_input[2] = 103
    text_input[3] = 32
    text_input[4] = 34
    text_input[5] = 111
    text_input[6] = 107
    text_input[7] = 34
    text_input[8] = 10
    let text_tokens = array_unchecked(20)
    let text_count = swan_lex(text_input, 9, text_tokens, 4, diagnostic)
    if text_count != 4
        return 15
    if text_tokens[0] != 1 or text_tokens[5] != 4 or text_tokens[7] != 4
        return 16
    if text_tokens[10] != 5 or text_tokens[15] != 0
        return 17
    let invalid_utf8 = array_unchecked(1)
    invalid_utf8[0] = 192
    let invalid_count = swan_lex(invalid_utf8, 1, text_tokens, 4, diagnostic)
    if invalid_count != 0 - 1 or diagnostic[0] != 1
        return 18
    let invalid_character = array_unchecked(1)
    invalid_character[0] = 64
    let invalid_character_count = swan_lex(invalid_character, 1, text_tokens, 4, diagnostic)
    if invalid_character_count != 0 - 2 or diagnostic[0] != 2
        return 19
    let unicode_source = array_unchecked(5)
    unicode_source[0] = 99
    unicode_source[1] = 97
    unicode_source[2] = 102
    unicode_source[3] = 195
    unicode_source[4] = 169
    let unicode_count = swan_lex(unicode_source, 5, text_tokens, 4, diagnostic)
    if unicode_count != 2 or text_tokens[0] != 1 or text_tokens[2] != 5
        return 23
    let bom_source = array_unchecked(5)
    bom_source[0] = 239
    bom_source[1] = 187
    bom_source[2] = 191
    bom_source[3] = 105
    bom_source[4] = 102
    let bom_count = swan_lex(bom_source, 5, text_tokens, 4, diagnostic)
    if bom_count != 2 or text_tokens[0] != 15 or text_tokens[1] != 3
        return 24
    if text_tokens[3] != 1 or text_tokens[4] != 1
        return 25
    return 0
out
"$SWAN" "$TMP/swanc-lexer-test.swan" --emit-asm -o "$TMP/swanc-lexer-test.s"
${CC:-cc} -x assembler "$TMP/swanc-lexer-test.s" -o "$TMP/swanc-lexer-test"
"$TMP/swanc-lexer-test"
printf 'PASS: Compiler 2 Swan lexer tokenization and exact keyword boundaries\n'
