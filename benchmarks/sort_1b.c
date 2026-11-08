#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define COUNT 1000000000LL
int main(void) {
    uint32_t *values = (uint32_t *)malloc((size_t)COUNT * sizeof(uint32_t));
    if (!values) { perror("malloc"); return 2; }
    int64_t state = 1;
    for (int64_t i = 0; i < COUNT; ++i) { state = (state * 48271) % 2147483647; values[i] = (uint32_t)state; }
    for (int64_t start = COUNT / 2 - 1; start >= 0; --start) {
        int64_t root = start;
        for (;;) {
            int64_t child = root * 2 + 1;
            if (child >= COUNT) break;
            if (child + 1 < COUNT && values[child] < values[child + 1]) ++child;
            if (values[root] >= values[child]) break;
            uint32_t t = values[root]; values[root] = values[child]; values[child] = t; root = child;
        }
    }
    for (int64_t end = COUNT - 1; end > 0; --end) {
        uint32_t t = values[0]; values[0] = values[end]; values[end] = t;
        int64_t root = 0;
        for (;;) {
            int64_t child = root * 2 + 1;
            if (child >= end) break;
            if (child + 1 < end && values[child] < values[child + 1]) ++child;
            if (values[root] >= values[child]) break;
            uint32_t v = values[root]; values[root] = values[child]; values[child] = v; root = child;
        }
    }
    int sorted = 1;
    for (int64_t i = 1; i < COUNT; ++i) { if (values[i - 1] > values[i]) { sorted = 0; break; } }
    printf("%d\n", sorted); free(values); return sorted ? 0 : 1;
}
