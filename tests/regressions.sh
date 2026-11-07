#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/regressions"
CC=${CC:-cc}
mkdir -p "$TMP"
expect_rejected() {
    name=$1
    if "$SWAN" "$TMP/$name.swan" -o "$TMP/$name.c" >/dev/null 2>&1; then
        echo "FAIL: $name should have been rejected" >&2
        exit 1
    fi
    printf 'PASS: rejected %s\n' "$name"
}
cat > "$TMP/missing-return.swan" <<'SWAN'
fn calculate(flag: bool) -> int {
    if flag { return 1; }
}
fn main() -> int { return 0; }
SWAN
expect_rejected missing-return
cat > "$TMP/void-variable.swan" <<'SWAN'
fn nothing() -> void { return; }
fn main() -> int {
    let result = nothing();
    return 0;
}
SWAN
expect_rejected void-variable
cat > "$TMP/void-parameter.swan" <<'SWAN'
fn invalid(value: void) -> int { return 0; }
fn main() -> int { return 0; }
SWAN
expect_rejected void-parameter
cat > "$TMP/void-field.swan" <<'SWAN'
struct Invalid { value: void; }
fn main() -> int { return 0; }
SWAN
expect_rejected void-field
cat > "$TMP/return-both-branches.swan" <<'SWAN'
fn choose(flag: bool) -> int {
    if flag { return 11; } else { return 22; }
}
fn main() -> int { log choose(true); return 0; }
SWAN
"$SWAN" "$TMP/return-both-branches.swan" -o "$TMP/return-both-branches.c"
"$CC" -std=c11 -Wall -Wextra -Werror "$TMP/return-both-branches.c" -o "$TMP/return-both-branches"
test "$("$TMP/return-both-branches")" = "11"
printf 'PASS: definite return across both branches\n'
cat > "$TMP/forward-struct.swan" <<'SWAN'
struct Bird { wing: Wing; }
struct Wing { size: int; }
fn main() -> int { return 0; }
SWAN
"$SWAN" "$TMP/forward-struct.swan" -o "$TMP/forward-struct.c"
"$CC" -std=c11 -Wall -Wextra -Werror "$TMP/forward-struct.c" -o "$TMP/forward-struct"
printf 'PASS: forward struct dependency ordering\n'
cat > "$TMP/struct-cycle.swan" <<'SWAN'
struct First { second: Second; }
struct Second { first: First; }
fn main() -> int { return 0; }
SWAN
expect_rejected struct-cycle
cat > "$TMP/identifier-collisions.swan" <<'SWAN'
fn printf() -> int { return 8; }
fn main() -> int {
    var printf: int = 3;
    log printf;
    log printf();
    return 0;
}
SWAN
"$SWAN" "$TMP/identifier-collisions.swan" -o "$TMP/identifier-collisions.c"
"$CC" -std=c11 -Wall -Wextra -Werror "$TMP/identifier-collisions.c" -o "$TMP/identifier-collisions"
test "$("$TMP/identifier-collisions")" = "3
8"
printf 'PASS: generated C identifier isolation\n'
cat > "$TMP/immutable-string.swan" <<'SWAN'
fn echo(value: string) -> string { return value; }
fn main() -> int {
    let greeting = "Hello";
    log greeting;
    log echo(greeting);
    return 0;
}
SWAN
"$SWAN" "$TMP/immutable-string.swan" -o "$TMP/immutable-string.c"
"$CC" -std=c11 -Wall -Wextra -Werror "$TMP/immutable-string.c" -o "$TMP/immutable-string"
test "$("$TMP/immutable-string")" = "Hello
Hello"
printf 'PASS: immutable string and parameter declarations\n'
cat > "$TMP/nested-shadowing.swan" <<'SWAN'
fn main() -> int {
    var value: int = 1;
    if true {
        var value: int = 2;
        log value;
    }
    log value;
    return 0;
}
SWAN
"$SWAN" "$TMP/nested-shadowing.swan" -o "$TMP/nested-shadowing.c"
"$CC" -std=c11 -Wall -Wextra -Werror "$TMP/nested-shadowing.c" -o "$TMP/nested-shadowing"
test "$("$TMP/nested-shadowing")" = "2
1"
printf 'PASS: nested symbol shadowing and scope restoration\\n'
cat > "$TMP/escaped-string.swan" <<'SWAN'
fn main() -> int {
    log "line\\nquote\\\"slash\\\\";
    return 0;
}
SWAN
"$SWAN" "$TMP/escaped-string.swan" -o "$TMP/escaped-string.c"
"$CC" -std=c11 -Wall -Wextra -Werror "$TMP/escaped-string.c" -o "$TMP/escaped-string"
test "$("$TMP/escaped-string")" = "line
quote\"slash\\"
printf 'PASS: escaped string literal generation\\n'
printf 'PASS: compiler regression suite\n'
