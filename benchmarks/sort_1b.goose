// Same 1B Park-Miller input and in-place heapsort as sort_1b.swan.
import std;

fn main() {
    let count = 1000000000;
    var values: u32[..] = [..count];
    var state: i64 = 1;
    for i in count {
        state = (state * 48271) % 2147483647;
        values.push(state as u32);
    }
    var start = count / 2 - 1;
    while start >= 0 {
        var root = start;
        var active = true;
        while active {
            var child = root * 2 + 1;
            if child < count {
                if child + 1 < count && values[child] < values[child + 1] { child++; }
                if values[root] < values[child] {
                    let temp = values[root]; values[root] = values[child]; values[child] = temp;
                    root = child;
                } else { active = false; }
            } else { active = false; }
        }
        start--;
    }
    var end = count - 1;
    while end > 0 {
        let temp = values[0]; values[0] = values[end]; values[end] = temp;
        var root = 0;
        var active = true;
        while active {
            var child = root * 2 + 1;
            if child < end {
                if child + 1 < end && values[child] < values[child + 1] { child++; }
                if values[root] < values[child] {
                    let swap = values[root]; values[root] = values[child]; values[child] = swap;
                    root = child;
                } else { active = false; }
            } else { active = false; }
        }
        end--;
    }
    var sorted = true;
    for i in 1..count { if values[i - 1] > values[i] { sorted = false; } }
    print(if sorted { 1 } else { 0 });
}
