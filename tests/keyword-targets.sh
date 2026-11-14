#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swanc"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM

cat > "$TMP/loop.swan" <<'SWAN'
func value() -> int:
    return 4

func enabled() -> bool:
    return true

func disabled() -> bool:
    return false

func before(value: int, limit: int) -> bool:
    return value < limit and not (value == limit) or value < 0

func main() -> int:
    const limit = value()
    var count = 0
    loop:
        count = count + 1
        if enabled() and not disabled() and before(count, limit):
            continue
        else:
            pass
            break
    return count
SWAN

"$BIN" build "$TMP/loop.swan" --target x86_64 --warnings-as-errors -o "$TMP/native"
set +e
"$TMP/native"
status=$?
set -e
test "$status" -eq 4
printf 'PASS: loop, const, var, func, if/else, true/false, and/or/not, continue, break, pass, return on x86-64\n'

"$BIN" build "$TMP/loop.swan" --target aarch64 --emit-asm -o "$TMP/aarch64.s"
clang --target=aarch64-unknown-linux-gnu -c -x assembler "$TMP/aarch64.s" -o "$TMP/aarch64.o"
readelf -h "$TMP/aarch64.o" | grep -q 'AArch64'
printf 'PASS: shared keyword control flow emits AArch64 ELF\n'

"$BIN" build "$TMP/loop.swan" --target riscv64 --emit-asm -o "$TMP/riscv64.s"
clang --target=riscv64-unknown-linux-gnu -c -x assembler "$TMP/riscv64.s" -o "$TMP/riscv64.o"
readelf -h "$TMP/riscv64.o" | grep -q 'RISC-V'
printf 'PASS: shared keyword control flow emits RISC-V64 ELF\n'

"$BIN" build "$TMP/loop.swan" --target wasm32 -o "$TMP/loop.wasm"
node -e 'const fs=require("fs"); const bytes=fs.readFileSync(process.argv[1]); WebAssembly.instantiate(bytes).then(({instance})=>{if(instance.exports.main()!==4n)process.exit(1);}).catch(e=>{console.error(e);console.error("WASM_HEX="+bytes.toString("hex"));process.exit(1);});' "$TMP/loop.wasm"
printf 'PASS: shared keyword control flow executes as WASM32\n'

"$BIN" build "$TMP/loop.swan" --target wasm64 -o "$TMP/loop64.wasm"
python3 - "$TMP/loop64.wasm" <<'PY'
import sys
data=open(sys.argv[1],"rb").read()
assert data[:8] == b"\0asm\1\0\0\0"
pos=8
def leb(buf, p):
    value=shift=0
    while True:
        byte=buf[p]; p+=1
        value|=(byte&127)<<shift
        if not byte&128: return value,p
        shift+=7
found=False
while pos<len(data):
    section=data[pos]; pos+=1
    size,pos=leb(data,pos); end=pos+size
    if section==5:
        count,pos=leb(data,pos)
        flags=data[pos]
        assert count==1 and flags&4
        found=True
    pos=end
assert found
PY
printf 'PASS: shared keyword control flow emits WASM64 memory64\n'

cat > "$TMP/loop-armv7.swan" <<'SWAN'
func main() -> i32:
    var count: i32 = 0
    loop:
        count = count + 1
        if count < 4:
            continue
        else:
            break
    return count
SWAN
"$BIN" build "$TMP/loop-armv7.swan" --target armv7 -o "$TMP/armv7.o"
readelf -h "$TMP/armv7.o" | grep -q 'ARM'
printf 'PASS: shared keyword control flow emits ARMv7 ELF\n'
