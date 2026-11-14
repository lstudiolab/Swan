#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swanc"
TMP="$ROOT/build/test/std-collections"
mkdir -p "$TMP"

cat > "$TMP/program.swan" <<'SWAN'
import std_collections

func main() -> int:
    var values = array_u32(6)
    values[0] = 9
    values[1] = 2
    values[2] = 7
    values[3] = 2
    values[4] = 5
    values[5] = 1
    std:print >> std:array_sum(values, 6)
    std:print >> std:array_min(values, 6)
    std:print >> std:array_max(values, 6)
    std:print >> std:array_contains(values, 6, 7)
    std:print >> std:array_index_of(values, 6, 2)
    std:print >> std:array_count(values, 6, 2)
    std:print >> std:array_average(values, 6)
    std:print >> std:array_is_sorted(values, 6)
    std:array_insertion_sort(values, 6)
    std:print >> std:array_is_sorted(values, 6)
    std:print >> std:array_binary_search(values, 6, 7)
    std:print >> values[0]
    std:print >> values[5]
    std:array_reverse(values, 6)
    std:print >> values[0]
    std:print >> values[5]
    return 0
SWAN

"$SWAN" build "$TMP/program.swan" -o "$TMP/program"
cat > "$TMP/expected.txt" <<'EXPECTED'
26
1
9
1
1
2
4
0
1
4
1
9
9
1
EXPECTED
"$TMP/program" > "$TMP/actual.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"
printf 'PASS: Swan-authored u32 array and sequence algorithms\n'
