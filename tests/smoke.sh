#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/test"
mkdir -p "$TMP"

cat > "$TMP/hello.swan" <<'SWAN'
fn main():
    log "Hello"
    return 0
SWAN
"$SWAN" "$TMP/hello.swan" -o "$TMP/hello.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/hello.c" -o "$TMP/hello"
test "$("$TMP/hello")" = "Hello"
printf 'PASS: indentation-based hello program\n'

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
"$SWAN" "$TMP/control.swan" -o "$TMP/control.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/control.c" -o "$TMP/control"
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
"$SWAN" "$TMP/multi-dedent.swan" -o "$TMP/multi-dedent.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/multi-dedent.c" -o "$TMP/multi-dedent"
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
"$SWAN" "$TMP/functions.swan" -o "$TMP/functions.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/functions.c" -o "$TMP/functions"
test "$("$TMP/functions")" = "42"
printf 'PASS: inferred integer return type\n'

cat > "$TMP/structs.swan" <<'SWAN'
struct Point:
    x: int
    y: float
fn main():
    return 0
SWAN
"$SWAN" "$TMP/structs.swan" -o "$TMP/structs.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/structs.c" -o "$TMP/structs"
printf 'PASS: indented struct declarations\n'

cat > "$TMP/invalid.swan" <<'SWAN'
fn main():
    log missing
    return 0
SWAN
if "$SWAN" "$TMP/invalid.swan" -o "$TMP/invalid.c" 2>/dev/null; then
  echo 'FAIL: semantic error accepted' >&2
  exit 1
fi
printf 'PASS: semantic errors rejected\n'

cat > "$TMP/identifier-location.swan" <<'SWAN'
fn main():
    log missing
    return 0
SWAN
if "$SWAN" "$TMP/identifier-location.swan" -o "$TMP/identifier-location.c" 2>"$TMP/identifier-location.err"; then
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
"$SWAN" "$TMP/keyword-operators.swan" -o "$TMP/keyword-operators.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/keyword-operators.c" -o "$TMP/keyword-operators"
test "$("$TMP/keyword-operators")" = "keyword operators preserved"
printf 'PASS: keyword operators and primitive types\n'
