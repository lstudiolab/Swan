#ifndef BREW_RUNTIME_THREAD_POOL_H
#define BREW_RUNTIME_THREAD_POOL_H

/*
 * Bounded POSIX worker pool. Submission never blocks: a full queue returns
 * false so callers can apply backpressure instead of growing memory forever.
 * Shutdown joins workers; queued jobs are either drained or discarded.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*BrewTaskFunction)(void *);

typedef struct {
    BrewTaskFunction function;
    void *context;
} BrewTask;

typedef struct {
    pthread_t *workers;
    size_t worker_count, workers_started;
    BrewTask *queue;
    size_t queue_capacity, queue_head, queue_length;
    pthread_mutex_t mutex;
    pthread_cond_t ready;
    bool initialized, stopping, drain;
} BrewThreadPool;

static inline void *brew_thread_pool_worker(void *argument) {
    BrewThreadPool *pool = (BrewThreadPool *)argument;
    for (;;) {
        pthread_mutex_lock(&pool->mutex);
        while (!pool->queue_length && !pool->stopping)
            pthread_cond_wait(&pool->ready, &pool->mutex);
        if (pool->stopping && (!pool->drain || !pool->queue_length)) {
            pthread_mutex_unlock(&pool->mutex);
            return NULL;
        }
        BrewTask task = pool->queue[pool->queue_head];
        pool->queue_head = (pool->queue_head + 1) % pool->queue_capacity;
        pool->queue_length--;
        pthread_mutex_unlock(&pool->mutex);
        task.function(task.context);
    }
}

static inline bool brew_thread_pool_init(BrewThreadPool *pool,
                                          size_t worker_count,
                                          size_t queue_capacity) {
    if (!pool || !worker_count || !queue_capacity) return false;
    *pool = (BrewThreadPool){0};
    if (worker_count > SIZE_MAX / sizeof(*pool->workers) ||
        queue_capacity > SIZE_MAX / sizeof(*pool->queue)) return false;
    pool->workers = (pthread_t *)calloc(worker_count, sizeof(*pool->workers));
    pool->queue = (BrewTask *)calloc(queue_capacity, sizeof(*pool->queue));
    if (!pool->workers || !pool->queue) goto fail_memory;
    if (pthread_mutex_init(&pool->mutex, NULL) != 0) goto fail_memory;
    if (pthread_cond_init(&pool->ready, NULL) != 0) {
        pthread_mutex_destroy(&pool->mutex);
        goto fail_memory;
    }
    pool->worker_count = worker_count;
    pool->queue_capacity = queue_capacity;
    pool->initialized = true;
    for (size_t i = 0; i < worker_count; i++) {
        if (pthread_create(&pool->workers[i], NULL,
                           brew_thread_pool_worker, pool) != 0) {
            pthread_mutex_lock(&pool->mutex);
            pool->stopping = true;
            pool->drain = false;
            pthread_cond_broadcast(&pool->ready);
            pthread_mutex_unlock(&pool->mutex);
            for (size_t j = 0; j < pool->workers_started; j++)
                pthread_join(pool->workers[j], NULL);
            pthread_cond_destroy(&pool->ready);
            pthread_mutex_destroy(&pool->mutex);
            goto fail_memory;
        }
        pool->workers_started++;
    }
    return true;

fail_memory:
    free(pool->workers);
    free(pool->queue);
    *pool = (BrewThreadPool){0};
    return false;
}

/* False means invalid task, stopped pool, or full queue. */
static inline bool brew_thread_pool_submit(BrewThreadPool *pool,
                                            BrewTaskFunction function,
                                            void *context) {
    if (!pool || !pool->initialized || !function) return false;
    pthread_mutex_lock(&pool->mutex);
    if (pool->stopping || pool->queue_length == pool->queue_capacity) {
        pthread_mutex_unlock(&pool->mutex);
        return false;
    }
    size_t tail = (pool->queue_head + pool->queue_length) %
                  pool->queue_capacity;
    pool->queue[tail] = (BrewTask){ function, context };
    pool->queue_length++;
    pthread_cond_signal(&pool->ready);
    pthread_mutex_unlock(&pool->mutex);
    return true;
}

/* drain=true completes queued work; false discards work not yet started. */
static inline void brew_thread_pool_shutdown(BrewThreadPool *pool, bool drain) {
    if (!pool || !pool->initialized) return;
    pthread_mutex_lock(&pool->mutex);
    pool->stopping = true;
    pool->drain = drain;
    if (!drain) {
        pool->queue_head = 0;
        pool->queue_length = 0;
    }
    pthread_cond_broadcast(&pool->ready);
    pthread_mutex_unlock(&pool->mutex);
    for (size_t i = 0; i < pool->workers_started; i++)
        pthread_join(pool->workers[i], NULL);
    pool->workers_started = 0;
    pthread_cond_destroy(&pool->ready);
    pthread_mutex_destroy(&pool->mutex);
    pool->initialized = false;
}

static inline void brew_thread_pool_destroy(BrewThreadPool *pool) {
    if (!pool) return;
    if (pool->initialized) brew_thread_pool_shutdown(pool, true);
    free(pool->workers);
    free(pool->queue);
    *pool = (BrewThreadPool){0};
}

#ifdef __cplusplus
}
#endif
#endif
