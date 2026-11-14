// 1,000,000,000 deterministic u32 values, sorted in place with max-heapsort.
import std;
const N = 1000000000;

fn main() {
    var values: u32[] = [0; N];
    var state: u64 = 305419896;

    for i in 0..N {
        state = (state * 1664525 + 1013904223) % 4294967296;
        values[i] = state as u32;
    }

    var start = N / 2 - 1;
    while start >= 0 {
        var root = start;
        while root * 2 + 1 < N {
            let left = root * 2 + 1;
            var largest = left;
            let right = left + 1;
            if right < N && values[right] > values[left] { largest = right; }
            if values[root] < values[largest] {
                let temporary = values[root];
                values[root] = values[largest];
                values[largest] = temporary;
                root = largest;
            } else { break; }
        }
        start -= 1;
    }

    var end = N - 1;
    while end > 0 {
        let temporary = values[0];
        values[0] = values[end];
        values[end] = temporary;
        end -= 1;

        var root = 0;
        while root * 2 + 1 <= end {
            let left = root * 2 + 1;
            var largest = left;
            let right = left + 1;
            if right <= end && values[right] > values[left] { largest = right; }
            if values[root] < values[largest] {
                let swap_value = values[root];
                values[root] = values[largest];
                values[largest] = swap_value;
                root = largest;
            } else { break; }
        }
    }

    var checksum: u64 = 0;
    for i in 0..N {
        if i > 0 && values[i - 1] > values[i] { abort("sort order invalid"); }
        checksum += values[i] as u64;
    }
    print(checksum);
}
