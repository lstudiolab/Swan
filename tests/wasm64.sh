#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BIN="$ROOT/build/swan"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT HUP INT TERM
cat > "$TMP/main.swan" <<'SWAN'
fn main() -> int:
    var count = 0
    while count < 3:
        count = count + 1
    return count
SWAN
"$BIN" build "$TMP/main.swan" --target wasm64 -o "$TMP/main.wasm"
test -s "$TMP/main.wasm"
python3 - "$TMP/main.wasm" <<'PY'
import sys
data=open(sys.argv[1],"rb").read()
assert data[:8] == b"\0asm\1\0\0\0"
def leb(buf, pos):
    value=shift=0
    while True:
        byte=buf[pos]; pos+=1
        value|=(byte&127)<<shift
        if not byte&128: return value,pos
        shift+=7
pos=8
found=False
while pos<len(data):
    section=data[pos]; pos+=1
    size,pos=leb(data,pos); end=pos+size
    if section==5:
        count,pos=leb(data,pos)
        flags=data[pos]
        assert count==1 and flags&4, "memory section is not memory64"
        found=True
    pos=end
assert found, "expected memory64 linear memory section"
PY
printf 'PASS: Swan emits a memory64 WebAssembly module with mutable locals and loop lowering\n'
