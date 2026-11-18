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
    own var memory: i64 = raw.alloc(64)
    var result: i64 = 0
    if true:
        borrow let view: i64 = memory
        let same: i64 = identity(view)
        raw.fill(same, 0, 64)
        raw.store8(same, 65)
        raw.store16(same + 2, 513)
        raw.store32(same + 4, 65537)
        raw.store64(same + 8, 123456789)
        let byte_value: i64 = raw.load8(same)
        let word_value: i64 = raw.load16(same + 2)
        let dword_value: i64 = raw.load32(same + 4)
        let qword_value: i64 = raw.load64(same + 8)
        result = byte_value + word_value + dword_value + qword_value
    drop memory
    own var source: i64 = raw.heap(16)
    own var destination: i64 = move source
    drop destination
    let stack_memory: i64 = raw.stack(16)
    raw.store8(stack_memory, 77)
    result = result + raw.load8(stack_memory)
    own var region: i64 = raw.arena(64)
    let chunk: i64 = raw.arena_alloc(region, 16)
    raw.store64(chunk, 987654321)
    result = result + raw.load64(chunk)
    raw.arena_reset(region)
    if raw.arena_remaining(region) != 64:
        result = result + 1
    drop region
    return result - 1111177302
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


cat > "$TMP/use-after-move.swan" <<'SWAN'
import raw
func main() -> int:
    own var source: i64 = raw.alloc(8)
    own var target: i64 = move source
    let invalid: i64 = source
    drop target
    return 0
SWAN
if "$SWAN" build "$TMP/use-after-move.swan" -o "$TMP/use-after-move" 2>/dev/null; then
    echo "use-after-move unexpectedly compiled" >&2
    exit 1
fi

cat > "$TMP/borrow-drop.swan" <<'SWAN'
import raw
func main() -> int:
    own var owner: i64 = raw.alloc(8)
    borrow let view: i64 = owner
    drop owner
    drop owner
    return 0
SWAN
if "$SWAN" build "$TMP/borrow-drop.swan" -o "$TMP/borrow-drop" 2>/dev/null; then
    echo "drop while borrowed unexpectedly compiled" >&2
    exit 1
fi

cat > "$TMP/leaked-owner.swan" <<'SWAN'
import raw
func main() -> int:
    own var owner: i64 = raw.alloc(8)
    return 0
SWAN
if "$SWAN" build "$TMP/leaked-owner.swan" -o "$TMP/leaked-owner" 2>/dev/null; then
    echo "undropped owner unexpectedly compiled" >&2
    exit 1
fi

printf 'PASS: raw i64 access, own/borrow/move/drop, stack, heap, arena, and lifetime diagnostics\n'
