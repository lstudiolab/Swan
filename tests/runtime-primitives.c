#include "runtime/bytes.h"
#include "runtime/thread_pool.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sched.h>

static atomic_size_t completed;

static void count_task(void *context) {
    (void)context;
    atomic_fetch_add_explicit(&completed, 1, memory_order_relaxed);
}

int main(void) {
    BrewByteBuffer bytes;
    brew_bytes_init(&bytes);
    const char payload[] = "brew";
    assert(brew_bytes_append(&bytes, payload, sizeof(payload) - 1));
    assert(bytes.length == 4 && memcmp(bytes.data, "brew", 4) == 0);
    assert(brew_bytes_append(&bytes, bytes.data, bytes.length));
    assert(bytes.length == 8 && memcmp(bytes.data, "brewbrew", 8) == 0);
    assert(brew_bytes_resize(&bytes, 12, 0));
    assert(bytes.length == 12 && bytes.data[11] == 0);
    brew_bytes_clear(&bytes);
    assert(bytes.length == 0);
    brew_bytes_destroy(&bytes);

    /* Fixed caller-owned storage must never fall back to heap allocation. */
    BrewByteBuffer fixed;
    unsigned char fixed_storage[8];
    brew_bytes_init(&fixed);
    assert(brew_bytes_use_storage(&fixed, fixed_storage, sizeof(fixed_storage)));
    assert(brew_bytes_append(&fixed, "12345678", 8));
    assert(fixed.data == fixed_storage && fixed.length == 8);
    assert(!brew_bytes_append_byte(&fixed, '!'));
    assert(!brew_bytes_resize(&fixed, 9, 0));
    brew_bytes_clear(&fixed);
    assert(fixed.length == 0 && fixed.data == fixed_storage);
    brew_bytes_destroy(&fixed);
    fixed_storage[0] = 'S';
    assert(fixed_storage[0] == 'S');

    BrewThreadPool pool;
    atomic_init(&completed, 0);
    assert(brew_thread_pool_init(&pool, 4, 256));
    for (size_t i = 0; i < 1000; i++) {
        while (!brew_thread_pool_submit(&pool, count_task, NULL)) sched_yield();
    }
    brew_thread_pool_shutdown(&pool, true);
    assert(atomic_load_explicit(&completed, memory_order_relaxed) == 1000);
    brew_thread_pool_destroy(&pool);
    puts("Runtime primitive tests passed");
    return 0;
}
