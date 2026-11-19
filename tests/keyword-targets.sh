#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/brewc"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/loop.brew" <<'BREW'
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
BREW
"$BIN" build "$TMP/loop.brew" --target x86_64 --warnings-as-errors -o "$TMP/native"
set +e
"$TMP/native"
status=$?
set -e
test "$status" -eq 4
printf 'PASS: shared language semantics execute on x86-64\n'
"$BIN" build "$TMP/loop.brew" --target aarch64 --emit-asm -o "$TMP/aarch64.s"
clang --target=aarch64-unknown-linux-gnu -c -x assembler "$TMP/aarch64.s" -o "$TMP/aarch64.o"
readelf -h "$TMP/aarch64.o" | grep -q 'AArch64'
printf 'PASS: shared language semantics emit AArch64 objects\n'
"$BIN" build "$TMP/loop.brew" --target wasm64 -o "$TMP/loop64.wasm"
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
        assert count==1 and flags&4, "expected memory64 linear memory"
        found=True
    pos=end
assert found, "memory64 section missing"
PY
printf 'PASS: shared language semantics emit WebAssembly memory64\n'
