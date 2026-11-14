#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TMP="$ROOT/build/multi-source"
rm -rf "$TMP"
mkdir -p "$TMP/project"

cat > "$TMP/project/01_math.swan" <<'SWAN'
export C func swan_answer() -> int:
    return 42
out
SWAN

cat > "$TMP/project/02_main.swan" <<SWAN
import C "$TMP/project/c_api.h"
fn main() -> int:
    return swan_answer() + c_answer(42) - 126
out
SWAN

cat > "$TMP/project/c_api.h" <<'C'
#include <stdint.h>
int64_t c_answer(int64_t value);
C
cat > "$TMP/project/native.c" <<'C'
#include <stdint.h>
int64_t swan_answer(void);
int64_t c_answer(int64_t value) { return value + swan_answer(); }
C

"$ROOT/build/swanc" build "$TMP/project" -o "$TMP/project-app"
"$TMP/project-app"

"$ROOT/build/swanc" build "$TMP/project" --emit-asm -o "$TMP/project.asm"
grep -q 'swan_answer' "$TMP/project.asm"
grep -q 'c_answer' "$TMP/project.asm"
grep -q 'main' "$TMP/project.asm"

printf 'PASS: directory-wide Swan compilation and C sources bundled into one assembly file\n'
