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
    own var memory: i64 = raw.alloc_zeroed(64, 1)
    var result: i64 = 0
    if raw.load8(borrow memory) == 0:
        borrow let view: i64 = memory
        raw.fill(view, 0, 64)
        raw.store8(borrow view, 65)
        raw.store16(view + 2, 513)
        raw.store32(view + 4, 65537)
        raw.store64(view + 8, 123456789)
        raw.store64(view + 16, 98765)
        raw.copy(view + 24, view + 16, 8)
        raw.move_bytes(view + 32, view + 24, 8)
        let byte_value: i64 = raw.load8(view)
        let word_value: i64 = raw.load16(view + 2)
        let dword_value: i64 = raw.load32(view + 4)
        let qword_value: i64 = raw.load64(view + 8)
        let copied_value: i64 = raw.load64(view + 32)
        result = byte_value + word_value + dword_value + qword_value + copied_value
    own var numbers: u32_array = array_u32(4)
    numbers[0] = 42
    if numbers[0] == 42:
        borrow let number_view: u32_array = numbers
        result = result + number_view[0]
    drop numbers
    drop memory
    own var source: i64 = raw.heap(16)
    own var destination: i64 = move source
    own var resized: i64 = raw.resize(move destination, 32)
    drop resized
    own var released: i64 = raw.alloc(8)
    raw.release(move released)
    let stack_memory: i64 = raw.stack(16)
    raw.store8(stack_memory, 77)
    result = result + raw.load8(stack_memory)
    own var region: i64 = raw.arena(64)
    if raw.arena_remaining(borrow region) == 64:
        borrow let chunk: i64 = raw.arena_alloc(borrow region, 16)
        raw.store64(chunk, 987654321)
        result = result + raw.load64(chunk)
    raw.arena_reset(borrow region)
    if raw.arena_remaining(borrow region) != 64:
        result = result + 1
    raw.arena_release(move region)
    return result - 1111276109
SWAN

"$SWAN" build "$TMP/program.swan" -o "$TMP/program"
"$TMP/program"
"$SWAN" build "$TMP/program.swan" --emit-asm -o "$TMP/program-asm"
grep -q 'call malloc' "$TMP/program-asm/raw.s"
grep -q 'call calloc' "$TMP/program-asm/raw.s"
grep -q 'call realloc' "$TMP/program-asm/raw.s"
grep -q 'call memcpy' "$TMP/program-asm/raw.s"
grep -q 'call memmove' "$TMP/program-asm/raw.s"
grep -q 'call memset' "$TMP/program-asm/raw.s"
grep -q 'free@PLT' "$TMP/program-asm/raw.s"
grep -q 'free@PLT' "$TMP/program-asm/program.s"
grep -q 'movzx eax, BYTE PTR \[rax\]' "$TMP/program-asm/program.s"
grep -q 'movzx eax, WORD PTR \[rax\]' "$TMP/program-asm/program.s"
grep -q 'mov eax, DWORD PTR \[rax\]' "$TMP/program-asm/program.s"
grep -q 'mov rax, QWORD PTR \[rax\]' "$TMP/program-asm/program.s"
grep -q 'mov BYTE PTR \[r10\], al' "$TMP/program-asm/program.s"
grep -q 'mov WORD PTR \[r10\], ax' "$TMP/program-asm/program.s"
grep -q 'mov DWORD PTR \[r10\], eax' "$TMP/program-asm/program.s"
grep -q 'mov QWORD PTR \[r10\], rax' "$TMP/program-asm/program.s"
grep -q 'sub rsp, rax' "$TMP/program-asm/program.s"

cat > "$TMP/automatic.swan" <<'SWAN'
import raw

func main() -> int:
    own var values: u32_array = array_u32(32)
    values[0] = 42
    drop values
    return 0
SWAN
"$SWAN" build "$TMP/automatic.swan" --emit-asm -o "$TMP/automatic-asm"
grep -q 'calloc@PLT' "$TMP/automatic-asm/automatic.s"
test "$(grep -c 'free@PLT' "$TMP/automatic-asm/automatic.s")" -eq 1


cat > "$TMP/default.swan" <<'SWAN'
func main() -> int:
    let values = array_u32(32768)
    values[0] = 42
    return values[0] - 42
SWAN
"$SWAN" build "$TMP/default.swan" --emit-asm -o "$TMP/default.s"
grep -q 'free@PLT' "$TMP/default.s"


cat > "$TMP/raw-array-without-own.swan" <<'SWAN'
import raw
func main() -> int:
    let values = array_u32(8)
    return 0
SWAN
if "$SWAN" build "$TMP/raw-array-without-own.swan" -o "$TMP/raw-array-without-own" 2>/dev/null; then
    echo "raw array allocation without own unexpectedly compiled" >&2
    exit 1
fi

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

if "$SWAN" build "$TMP/program.swan" --target aarch64 -o "$TMP/unsupported-target" 2>"$TMP/unsupported-target.err"; then
    echo "raw mode unexpectedly compiled for AArch64" >&2
    exit 1
fi

cat > "$TMP/raw-target-without-intrinsics.swan" <<'SWAN'
import raw
func local_address() -> i64:
    return 0
func main() -> int:
    own var address: i64 = local_address()
    drop address
    return 0
SWAN
if "$SWAN" build "$TMP/raw-target-without-intrinsics.swan" --target aarch64 -o "$TMP/raw-target-without-intrinsics" 2>"$TMP/raw-target-without-intrinsics.err"; then
    echo "raw mode without external intrinsics unexpectedly compiled for AArch64" >&2
    exit 1
fi
grep -q 'import raw currently requires the native x86-64 backend' "$TMP/raw-target-without-intrinsics.err"


cat > "$TMP/double-drop.swan" <<'SWAN'
import raw
func main() -> int:
    own var owner: i64 = raw.alloc(8)
    drop owner
    drop owner
    return 0
SWAN
if "$SWAN" build "$TMP/double-drop.swan" -o "$TMP/double-drop" 2>/dev/null; then
    echo "double drop unexpectedly compiled" >&2
    exit 1
fi

cat > "$TMP/borrow-copy.swan" <<'SWAN'
import raw
func main() -> int:
    own var owner: i64 = raw.alloc(8)
    borrow let view: i64 = owner
    let copied: i64 = view
    drop owner
    return 0
SWAN
if "$SWAN" build "$TMP/borrow-copy.swan" -o "$TMP/borrow-copy" 2>/dev/null; then
    echo "borrow copied into ordinary variable unexpectedly compiled" >&2
    exit 1
fi

cat > "$TMP/borrow-escape.swan" <<'SWAN'
import raw
func identity(pointer: i64) -> i64:
    return pointer
func main() -> int:
    own var owner: i64 = raw.alloc(8)
    borrow let view: i64 = owner
    let escaped: i64 = identity(view)
    drop owner
    return 0
SWAN
if "$SWAN" build "$TMP/borrow-escape.swan" -o "$TMP/borrow-escape" 2>/dev/null; then
    echo "borrow escaped through an ordinary function call" >&2
    exit 1
fi

cat > "$TMP/arena-reset-live-borrow.swan" <<'SWAN'
import raw
func main() -> int:
    own var region: i64 = raw.arena(64)
    borrow let chunk: i64 = raw.arena_alloc(borrow region, 16)
    raw.arena_reset(borrow region)
    drop region
    return 0
SWAN
if "$SWAN" build "$TMP/arena-reset-live-borrow.swan" -o "$TMP/arena-reset-live-borrow" 2>/dev/null; then
    echo "arena reset with a live slice borrow unexpectedly compiled" >&2
    exit 1
fi

cat > "$TMP/stack-in-loop.swan" <<'SWAN'
import raw
func main() -> int:
    var index: i64 = 0
    while index < 4:
        let memory: i64 = raw.stack(16)
        raw.store8(memory, 1)
        index = index + 1
    return 0
SWAN
if "$SWAN" build "$TMP/stack-in-loop.swan" -o "$TMP/stack-in-loop" 2>/dev/null; then
    echo "stack allocation inside a loop unexpectedly compiled" >&2
    exit 1
fi

cat > "$TMP/own-without-raw.swan" <<'SWAN'
func main() -> int:
    own var owner: i64 = 0
    return 0
SWAN
if "$SWAN" build "$TMP/own-without-raw.swan" -o "$TMP/own-without-raw" 2>/dev/null; then
    echo "own without import raw unexpectedly compiled" >&2
    exit 1
fi


cat > "$TMP/move-while-borrowed.swan" <<'SWAN'
import raw
func main() -> int:
    own var owner: i64 = raw.alloc(8)
    borrow let view: i64 = owner
    own var target: i64 = move owner
    drop target
    return 0
SWAN
if "$SWAN" build "$TMP/move-while-borrowed.swan" -o "$TMP/move-while-borrowed" 2>/dev/null; then
    echo "move while borrowed unexpectedly compiled" >&2
    exit 1
fi

cat > "$TMP/branch-ownership.swan" <<'SWAN'
import raw
func main() -> int:
    own var owner: i64 = raw.alloc_zeroed(8, 1)
    if raw.load8(borrow owner) == 0:
        drop owner
    else:
        pass
    return 0
SWAN
if "$SWAN" build "$TMP/branch-ownership.swan" -o "$TMP/branch-ownership" 2>/dev/null; then
    echo "inconsistent branch ownership unexpectedly compiled" >&2
    exit 1
fi

cat > "$TMP/loop-ownership.swan" <<'SWAN'
import raw
func main() -> int:
    own var owner: i64 = raw.alloc(8)
    var count: i64 = 0
    while count < 1:
        drop owner
        count = count + 1
    return 0
SWAN
if "$SWAN" build "$TMP/loop-ownership.swan" -o "$TMP/loop-ownership" 2>/dev/null; then
    echo "ambiguous loop ownership unexpectedly compiled" >&2
    exit 1
fi


cat > "$TMP/stack-escape.swan" <<'SWAN'
import raw
func escape() -> i64:
    let pointer: i64 = raw.stack(16)
    let alias: i64 = pointer
    return alias
func main() -> int:
    let leaked: i64 = escape()
    return 0
SWAN
if "$SWAN" build "$TMP/stack-escape.swan" -o "$TMP/stack-escape" 2>/dev/null; then
    echo "stack pointer escaped its function unexpectedly" >&2
    exit 1
fi

printf 'PASS: raw i64 access, own/borrow/move/drop, stack, heap, arena, and lifetime diagnostics\n'
