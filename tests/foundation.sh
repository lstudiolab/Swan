#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/test/foundation"
mkdir -p "$TMP"

cat "$ROOT/stdlib/foundation.swan" > "$TMP/program.swan"
cat >> "$TMP/program.swan" <<'SWAN'
fn main() -> int {
    echo("Hello from Foundation");
    return 0;
}
SWAN

"$SWAN" "$TMP/program.swan" -o "$TMP/program.c"
${CC:-cc} -std=c11 -Wall -Wextra -Werror "$TMP/program.c" -o "$TMP/program"
test "$("$TMP/program")" = "Hello from Foundation"
printf 'PASS: Foundation.echo\n'
