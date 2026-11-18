#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/raw-memory"
mkdir -p "$TMP"

cat > "$TMP/program.swan" <<'SWAN'
import raw

func identity(pointer: i64) -> i64:
    return pointer

func main() -> int:
    let memory: i64 = raw.alloc(64)
    let same: i64 = identity(memory)
    raw.fill(same, 0, 64)
    raw.release(same)
    return 0
SWAN

"$SWAN" build "$TMP/program.swan" -o "$TMP/program"
"$TMP/program"
"$SWAN" build "$TMP/program.swan" --emit-asm -o "$TMP/program.s"
grep -q 'malloc@PLT' "$TMP/program.s"
grep -q 'memset@PLT' "$TMP/program.s"
grep -q 'free@PLT' "$TMP/program.s"

cat > "$TMP/automatic.swan" <<'SWAN'
import raw

func main() -> int:
    let values = array_u32(32)
    values[0] = 42
    return values[0] - 42
SWAN
"$SWAN" build "$TMP/automatic.swan" --emit-asm -o "$TMP/automatic.s"
if grep -q 'free@PLT' "$TMP/automatic.s"; then
    echo "raw mode unexpectedly emitted automatic array reclamation" >&2
    exit 1
fi


cat > "$TMP/default.swan" <<'SWAN'
func main() -> int:
    let values = array_u32(32)
    values[0] = 42
    return values[0] - 42
SWAN
"$SWAN" build "$TMP/default.swan" --emit-asm -o "$TMP/default.s"
grep -q 'free@PLT' "$TMP/default.s"

printf 'PASS: raw import, i64 pointer ABI, native allocation, byte fill, release, and mode-specific array lifetime behavior\n'
