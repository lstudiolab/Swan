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
int c_answer(int value);
C
cat > "$TMP/project/native.c" <<'C'
int swan_answer(void);
int c_answer(int value) { return value + swan_answer(); }
C

printf 'RUN: compile mixed Swan/C executable\n'
"$ROOT/build/swanc" build "$TMP/project" -o "$TMP/project-app"
printf 'RUN: execute mixed Swan/C executable\n'
"$TMP/project-app"

printf 'RUN: emit mixed Swan/C assembly\n'
"$ROOT/build/swanc" build "$TMP/project" --emit-asm -o "$TMP/project.asm"
printf 'ASSEMBLY SYMBOLS:\\n'
grep -E 'swan_answer|c_answer|main' "$TMP/project.asm" | head -30
grep -q 'swan_answer' "$TMP/project.asm" || { echo 'FAIL: Swan export missing from combined assembly' >&2; exit 1; }
grep -q 'c_answer' "$TMP/project.asm" || { echo 'FAIL: C symbol missing from combined assembly' >&2; exit 1; }
grep -q 'main' "$TMP/project.asm" || { echo 'FAIL: entry point missing from combined assembly' >&2; exit 1; }

# The same single-artifact promise applies to the portable target: multiple
# Swan source files become one executable WebAssembly module.
mkdir -p "$TMP/portable-project"
cat > "$TMP/portable-project/01_math.swan" <<'SWAN'
func answer() -> int:
    return 42
out
SWAN
cat > "$TMP/portable-project/02_main.swan" <<'SWAN'
func main() -> int:
    return answer() - 42
out
SWAN
"$ROOT/build/swanc" build "$TMP/portable-project" --target portable -o "$TMP/portable-project.wasm"
test -s "$TMP/portable-project.wasm"
node -e 'const fs=require("fs"); WebAssembly.instantiate(fs.readFileSync(process.argv[1])).then(({instance})=>{if(instance.exports.main()!==0n)process.exit(1);}).catch(e=>{console.error(e);process.exit(1);});' "$TMP/portable-project.wasm"
printf 'PASS: multiple Swan sources bundled into one portable executable module\n'

printf 'PASS: directory-wide Swan compilation and C sources bundled into one assembly file\n'
