#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define COUNT ((size_t)1000000000)

static void sift_down(uint32_t *values, size_t root, size_t end) {
    for (;;) {
        size_t left = root * 2 + 1;
        if (left > end) return;
        size_t largest = left;
        size_t right = left + 1;
        if (right <= end && values[right] > values[left]) largest = right;
        if (values[root] >= values[largest]) return;
        uint32_t temporary = values[root];
        values[root] = values[largest];
        values[largest] = temporary;
        root = largest;
    }
}

int main(void) {
    uint32_t *values = (uint32_t *)calloc(COUNT, sizeof(uint32_t));
    if (!values) { perror("calloc"); return 2; }

    uint32_t state = UINT32_C(305419896);
    for (size_t i = 0; i < COUNT; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        values[i] = state;
    }

    for (size_t start = COUNT / 2; start > 0; --start)
        sift_down(values, start - 1, COUNT - 1);

    for (size_t end = COUNT - 1; end > 0; --end) {
        uint32_t temporary = values[0];
        values[0] = values[end];
        values[end] = temporary;
        sift_down(values, 0, end - 1);
    }

    uint64_t checksum = 0;
    for (size_t i = 0; i < COUNT; ++i) {
        if (i && values[i - 1] > values[i]) {
            fputs("SORT_FAILED\n", stderr); free(values); return 3;
        }
        checksum += values[i];
    }
    printf("%" PRIu64 "\n", checksum);
    free(values);
    return 0;
}
