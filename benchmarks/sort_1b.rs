const COUNT: usize = 1_000_000_000;
fn main() {
    let mut values = vec![0u32; COUNT];
    let mut state: i64 = 1;
    for value in values.iter_mut() { state = (state * 48271) % 2_147_483_647; *value = state as u32; }
    for start in (0..COUNT / 2).rev() {
        let mut root = start;
        loop {
            let mut child = root * 2 + 1;
            if child >= COUNT { break; }
            if child + 1 < COUNT && values[child] < values[child + 1] { child += 1; }
            if values[root] >= values[child] { break; }
            values.swap(root, child); root = child;
        }
    }
    for end in (1..COUNT).rev() {
        values.swap(0, end);
        let mut root = 0usize;
        loop {
            let mut child = root * 2 + 1;
            if child >= end { break; }
            if child + 1 < end && values[child] < values[child + 1] { child += 1; }
            if values[root] >= values[child] { break; }
            values.swap(root, child); root = child;
        }
    }
    let sorted = values.windows(2).all(|pair| pair[0] <= pair[1]);
    println!("{}", if sorted { 1 } else { 0 });
    assert!(sorted);
}
