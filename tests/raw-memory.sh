#!/usr/bin/env sh
set -eux
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
    raw.store8(same, 65)
    raw.store16(same + 2, 513)
    raw.store32(same + 4, 65537)
    raw.store64(same + 8, 123456789)
    let byte_value: i64 = raw.load8(same)
    let word_value: i64 = raw.load16(same + 2)
    let dword_value: i64 = raw.load32(same + 4)
    let qword_value: i64 = raw.load64(same + 8)
    raw.release(same)
    return byte_value + word_value + dword_value + qword_value - 123522904
SWAN

"$SWAN" build "$TMP/program.swan" -o "$TMP/program"
"$TMP/program"
"$SWAN" build "$TMP/program.swan" --emit-asm -o "$TMP/program-asm"
grep -q 'call malloc' "$TMP/program-asm/raw.s"
grep -q 'call memset' "$TMP/program-asm/raw.s"
grep -q 'free@PLT' "$TMP/program-asm/program.s"

cat > "$TMP/automatic.swan" <<'SWAN'
import raw

func main() -> int:
    let values = array_u32(32)
    values[0] = 42
    return values[0] - 42
SWAN
"$SWAN" build "$TMP/automatic.swan" --emit-asm -o "$TMP/automatic-asm"
if grep -q 'free@PLT' "$TMP/automatic-asm/automatic.s"; then
    echo "raw mode unexpectedly emitted automatic array reclamation" >&2
    exit 1
fi


cat > "$TMP/default.swan" <<'SWAN'
func main() -> int:
    let values = array_u32(32768)
    values[0] = 42
    return values[0] - 42
SWAN
"$SWAN" build "$TMP/default.swan" --emit-asm -o "$TMP/default.s"
grep -q 'free@PLT' "$TMP/default.s"

printf 'PASS: raw import, i64 pointer ABI, native allocation, byte fill, release, and mode-specific array lifetime behavior\n'
