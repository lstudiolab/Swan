#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
"$ROOT/build.sh"
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/test"
mkdir -p "$TMP"
cat > "$TMP/hello.swan" <<'SWAN'
fn main() -> int { log "Hello"; return 0; }
SWAN
"$SWAN" "$TMP/hello.swan" -o "$TMP/hello.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/hello.c" -o "$TMP/hello"
test "$("$TMP/hello")" = "Hello"
printf 'PASS: hello program\n'
cat > "$TMP/control.swan" <<'SWAN'
fn main() -> int {
 var x: int = 0;
 while x < 3 { if x == 1 { log x; } else { log 0; } x = x + 1; }
 return 0;
}
SWAN
"$SWAN" "$TMP/control.swan" -o "$TMP/control.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/control.c" -o "$TMP/control"
test "$("$TMP/control")" = "0
1
0"
printf 'PASS: nested control flow\n'
cat > "$TMP/functions.swan" <<'SWAN'
fn answer() -> int { return 42; }
fn main() -> int { log answer(); return 0; }
SWAN
"$SWAN" "$TMP/functions.swan" -o "$TMP/functions.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/functions.c" -o "$TMP/functions"
test "$("$TMP/functions")" = "42"
printf 'PASS: function calls\n'
cat > "$TMP/invalid.swan" <<'SWAN'
fn main() -> int { log missing; return 0; }
SWAN
if "$SWAN" "$TMP/invalid.swan" -o "$TMP/invalid.c" 2>/dev/null; then
  echo 'FAIL: semantic error accepted' >&2
  exit 1
fi
printf 'PASS: semantic errors rejected\n'
