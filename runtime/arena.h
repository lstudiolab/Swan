#ifndef SWAN_ARENA_H
#define SWAN_ARENA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Caller-owned, fixed-capacity bump arena.
 *
 * The arena never allocates, reallocates, or frees memory. The backing storage
 * must outlive the arena and every allocation made from it. Reset/rewind
 * invalidates allocations after the selected position.
 */
typedef struct {
    unsigned char *data;
    size_t capacity;
    size_t used;
} SwanArena;

typedef size_t SwanArenaMark;

static inline bool swan_arena_init(SwanArena *arena, void *storage,
                                   size_t capacity) {
    if (!arena || (!storage && capacity)) return false;
    arena->data = (unsigned char *)storage;
    arena->capacity = capacity;
    arena->used = 0;
    return true;
}

static inline SwanArenaMark swan_arena_mark(const SwanArena *arena) {
    return arena ? arena->used : 0;
}

static inline bool swan_arena_rewind(SwanArena *arena, SwanArenaMark mark) {
    if (!arena || mark > arena->used) return false;
    arena->used = mark;
    return true;
}

static inline void swan_arena_reset(SwanArena *arena) {
    if (arena) arena->used = 0;
}

static inline size_t swan_arena_remaining(const SwanArena *arena) {
    return arena && arena->used <= arena->capacity
        ? arena->capacity - arena->used : 0;
}

static inline void *swan_arena_alloc_aligned(SwanArena *arena, size_t size,
                                              size_t alignment) {
    if (!arena || !alignment || (alignment & (alignment - 1)) != 0)
        return NULL;
    if (!size) size = 1;
    uintptr_t base = (uintptr_t)arena->data;
    if (arena->used > arena->capacity || base > UINTPTR_MAX - arena->used)
        return NULL;
    uintptr_t current = base + arena->used;
    size_t padding = (size_t)((0u - current) & (uintptr_t)(alignment - 1));
    if (padding > arena->capacity - arena->used) return NULL;
    size_t offset = arena->used + padding;
    if (size > arena->capacity - offset) return NULL;
    void *result = arena->data + offset;
    arena->used = offset + size;
    return result;
}

static inline void *swan_arena_alloc(SwanArena *arena, size_t size) {
    return swan_arena_alloc_aligned(arena, size, _Alignof(max_align_t));
}

static inline void *swan_arena_calloc(SwanArena *arena, size_t count,
                                       size_t size) {
    if (size && count > SIZE_MAX / size) return NULL;
    size_t bytes = count * size;
    void *result = swan_arena_alloc(arena, bytes);
    if (result) memset(result, 0, bytes);
    return result;
}

#ifdef __cplusplus
}
#endif
#endif
