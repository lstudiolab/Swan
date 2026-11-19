#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BREW="$ROOT/build/brewc"
TMP="$ROOT/build/test/std-collections"
mkdir -p "$TMP"

cat > "$TMP/program.brew" <<'BREW'
import std_collections

func main() -> int:
    var values = array_u32(6)
    values[0] = 9
    values[1] = 2
    values[2] = 7
    values[3] = 2
    values[4] = 5
    values[5] = 1
    print(array_sum(values, 6))
    print(array_min(values, 6))
    print(array_max(values, 6))
    print(array_contains(values, 6, 7))
    print(array_index_of(values, 6, 2))
    print(array_count(values, 6, 2))
    print(array_average(values, 6))
    print(array_is_sorted(values, 6))
    array_insertion_sort(values, 6)
    print(array_is_sorted(values, 6))
    print(array_binary_search(values, 6, 7))
    print(values[0])
    print(values[5])
    array_reverse(values, 6)
    print(values[0])
    print(values[5])
    var heap_values = array_u32(8)
    heap_values[0] = 12
    heap_values[1] = 3
    heap_values[2] = 19
    heap_values[3] = 3
    heap_values[4] = 0
    heap_values[5] = 8
    heap_values[6] = 14
    heap_values[7] = 1
    print(array_heap_sort(heap_values, 8))
    print(array_is_sorted(heap_values, 8))
    print(heap_values[0])
    print(heap_values[7])
    print(array_sum(values, -1))
    print(array_min(values, -1))
    print(array_max(values, -1))
    print(array_contains(values, -1, 1))
    print(array_index_of(values, -1, 1))
    print(array_count(values, -1, 1))
    print(array_average(values, -1))
    print(array_is_sorted(values, -1))
    print(array_fill(values, -1, 5))
    print(array_fill(values, 6, -1))
    print(array_fill(values, 6, 4294967296))
    print(array_copy(values, values, -1))
    print(array_reverse(values, -1))
    print(array_bubble_sort(values, -1))
    print(array_selection_sort(values, -1))
    print(array_insertion_sort(values, -1))
    print(array_heap_sort(values, -1))
    return 0
BREW

"$BREW" build "$TMP/program.brew" -o "$TMP/program"
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
8
1
0
19
0
0
0
0
-1
0
0
0
0
0
0
0
0
0
0
0
0
EXPECTED
"$TMP/program" > "$TMP/actual.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"
printf 'PASS: Brew-authored u32 array and sequence algorithms\n'
