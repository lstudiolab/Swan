#!/usr/bin/env sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SWAN="$ROOT/build/swan"
TMP="$ROOT/build/test/foundation"
mkdir -p "$TMP"

cat > "$TMP/program.swan" <<'SWAN'
import foundation
import foundation
fn main():
    foundation_hello()
    !printin >> abs_int(-9);
    !printin >> average_int(4, 7);
    !printin >> is_even(12);
    !printin >> is_odd(9);
    !printin >> min_int(8, 3);
    !printin >> max_int(8, 3);
    !printin >> clamp_int(15, 0, 10);
    !printin >> sign_int(-20);
    !printin >> square_int(5);
    !printin >> cube_int(3);
    !printin >> power_int(2, 8);
    !printin >> factorial_int(5);
    !printin >> sum_to_int(10);
    !printin >> in_range_int(5, 1, 9);
    return 0
SWAN

"$SWAN" build "$TMP/program.swan" -o "$TMP/program"
cat > "$TMP/expected.txt" <<'EXPECTED'
9
5
1
1
3
8
10
-1
25
27
256
120
55
1
EXPECTED
"$TMP/program" > "$TMP/actual.txt"
diff -u "$TMP/expected.txt" "$TMP/actual.txt"
printf 'PASS: imported Foundation helpers and !printin output\n'
