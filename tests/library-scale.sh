#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/library-scale"
rm -rf "$TMP"
mkdir -p "$TMP/first" "$TMP/second"

cat > "$TMP/first/entry.swan" <<'SWAN'
import helper
func entry_value() -> int:
    return helper_value()
SWAN
cat > "$TMP/second/helper.swan" <<'SWAN'
func helper_value() -> int:
    return 42
SWAN
cat > "$TMP/transitive.swan" <<'SWAN'
import entry
func main() -> int:
    return entry_value() - 42
SWAN
"$SWAN" build "$TMP/transitive.swan" --module-path "$TMP/first" -I "$TMP/second" -o "$TMP/transitive"
"$TMP/transitive"
printf 'PASS: transitive imports resolve across multiple module roots\n'

: > "$TMP/second/large.swan"
i=1100
while [ "$i" -gt 1 ]; do
    next=$((i + 1))
    if [ "$i" -eq 1100 ]; then
        printf 'func mass_%s() -> int:\n    return 42\n\n' "$i" >> "$TMP/second/large.swan"
    else
        printf 'func mass_%s() -> int:\n    return mass_%s()\n\n' "$i" "$next" >> "$TMP/second/large.swan"
    fi
    i=$((i - 1))
done
cat >> "$TMP/second/large.swan" <<'SWAN'
func mass_1() -> int:
    return mass_2()

func sum20(a1: int, a2: int, a3: int, a4: int, a5: int, a6: int, a7: int, a8: int, a9: int, a10: int, a11: int, a12: int, a13: int, a14: int, a15: int, a16: int, a17: int, a18: int, a19: int, a20: int) -> int:
    return a1 + a2 + a3 + a4 + a5 + a6 + a7 + a8 + a9 + a10 + a11 + a12 + a13 + a14 + a15 + a16 + a17 + a18 + a19 + a20
SWAN
cat > "$TMP/large-program.swan" <<'SWAN'
import large
func main() -> int:
    return mass_1() + sum20(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20) - 252
SWAN
"$SWAN" build "$TMP/large-program.swan" -I "$TMP/second" -o "$TMP/large-program"
"$TMP/large-program"
printf 'PASS: 1,101 imported functions and 20-argument IR calls\n'

cat > "$TMP/large-locals.swan" <<'SWAN'
func main() -> int:
SWAN
i=1
while [ "$i" -le 1100 ]; do
    printf '    let value_%s = %s\n' "$i" "$i" >> "$TMP/large-locals.swan"
    i=$((i + 1))
done
cat >> "$TMP/large-locals.swan" <<'SWAN'
    print(value_1100)
    return 0
SWAN
"$SWAN" build "$TMP/large-locals.swan" -o "$TMP/large-locals"
actual=$("$TMP/large-locals")
test "$actual" = "1100"
printf 'PASS: 1,100 local declarations through the AST compatibility backend\n'
