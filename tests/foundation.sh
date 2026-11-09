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
    echo_int(abs_int(-9))
    echo_int(average_int(4, 7))
    echo_bool(is_even(12))
    echo_bool(is_odd(9))
    echo_int(min_int(8, 3))
    echo_int(max_int(8, 3))
    echo_int(clamp_int(15, 0, 10))
    echo_int(sign_int(-20))
    echo_int(square_int(5))
    echo_int(cube_int(3))
    echo_int(power_int(2, 8))
    echo_int(factorial_int(5))
    echo_int(sum_to_int(10))
    echo_bool(in_range_int(5, 1, 9))
    return 0
out
SWAN

"$SWAN" build "$TMP/program.swan" -o "$TMP/program"
cat > "$TMP/expected.txt" <<'EXPECTED'
Foundation standard library ready
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
printf 'PASS: imported Foundation standard library and echo! intrinsic\n'
