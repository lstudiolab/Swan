const COUNT: usize = 1_000_000_000;

fn sift_down(values: &mut [u32], mut root: usize, end: usize) {
    loop {
        let left = root * 2 + 1;
        if left > end { return; }
        let mut largest = left;
        let right = left + 1;
        if right <= end && values[right] > values[left] { largest = right; }
        if values[root] >= values[largest] { return; }
        values.swap(root, largest);
        root = largest;
    }
}

fn main() {
    let mut values = Vec::<u32>::with_capacity(COUNT);
    let mut state = 305_419_896u64;
    for _ in 0..COUNT {
        state = (state * 1_664_525 + 1_013_904_223) % 4_294_967_296;
        values.push(state as u32);
    }

    for start in (0..COUNT / 2).rev() {
        sift_down(&mut values, start, COUNT - 1);
    }
    for end in (1..COUNT).rev() {
        values.swap(0, end);
        sift_down(&mut values, 0, end - 1);
    }

    let mut checksum = 0u64;
    for i in 0..COUNT {
        if i > 0 && values[i - 1] > values[i] {
            eprintln!("SORT_FAILED");
            std::process::exit(3);
        }
        checksum += values[i] as u64;
    }
    println!("{checksum}");
}
