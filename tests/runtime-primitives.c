#include "runtime/bytes.h"
#include "runtime/thread_pool.h"
#include <assert.h>
#include <stdatomic.h>
#include <string.h>
#include <sched.h>

static atomic_size_t completed;

static void count_task(void *context) {
    (void)context;
    atomic_fetch_add_explicit(&completed, 1, memory_order_relaxed);
}

int main(void) {
    SwanByteBuffer bytes;
    swan_bytes_init(&bytes);
    const char payload[] = "swan";
    assert(swan_bytes_append(&bytes, payload, sizeof(payload) - 1));
    assert(bytes.length == 4 && memcmp(bytes.data, "swan", 4) == 0);
    assert(swan_bytes_append(&bytes, bytes.data, bytes.length));
    assert(bytes.length == 8 && memcmp(bytes.data, "swanswan", 8) == 0);
    assert(swan_bytes_resize(&bytes, 12, 0));
    assert(bytes.length == 12 && bytes.data[11] == 0);
    swan_bytes_clear(&bytes);
    assert(bytes.length == 0);
    swan_bytes_destroy(&bytes);

    SwanThreadPool pool;
    atomic_init(&completed, 0);
    assert(swan_thread_pool_init(&pool, 4, 256));
    for (size_t i = 0; i < 1000; i++) {
        while (!swan_thread_pool_submit(&pool, count_task, NULL)) sched_yield();
    }
    swan_thread_pool_shutdown(&pool, true);
    assert(atomic_load_explicit(&completed, memory_order_relaxed) == 1000);
    swan_thread_pool_destroy(&pool);
    puts("Runtime primitive tests passed");
    return 0;
}
