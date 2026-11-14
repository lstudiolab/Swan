import Foundation
import Glibc

let count = 1_000_000_000
let values = UnsafeMutablePointer<UInt32>.allocate(capacity: count)
values.initialize(repeating: 0, count: count)
defer {
    values.deinitialize(count: count)
    values.deallocate()
}

var state: UInt64 = 305_419_896
for i in 0..<count {
    state = (state * 1_664_525 + 1_013_904_223) % 4_294_967_296
    values[i] = UInt32(state)
}

@inline(__always)
func siftDown(_ values: UnsafeMutablePointer<UInt32>, _ initialRoot: Int, _ end: Int) {
    var root = initialRoot
    while true {
        let left = root * 2 + 1
        if left > end { return }
        var largest = left
        let right = left + 1
        if right <= end && values[right] > values[left] { largest = right }
        if values[root] >= values[largest] { return }
        let temporary = values[root]
        values[root] = values[largest]
        values[largest] = temporary
        root = largest
    }
}

for start in stride(from: count / 2 - 1, through: 0, by: -1) {
    siftDown(values, start, count - 1)
}
for end in stride(from: count - 1, through: 1, by: -1) {
    let temporary = values[0]
    values[0] = values[end]
    values[end] = temporary
    siftDown(values, 0, end - 1)
}

var checksum: UInt32 = 0
for i in 0..<count {
    if i > 0 && values[i - 1] > values[i] {
        fputs("SORT_FAILED\n", stderr)
        exit(3)
    }
    checksum = checksum &+ values[i]
}
print(checksum)
