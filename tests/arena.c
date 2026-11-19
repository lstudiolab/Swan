#include "brew/arena.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void) {
    union {
        max_align_t align;
        unsigned char bytes[256];
    } storage;
    BrewArena arena;
    assert(brew_arena_init(&arena, storage.bytes, sizeof(storage.bytes)));
    assert(brew_arena_remaining(&arena) == sizeof(storage.bytes));

    BrewArenaMark start = brew_arena_mark(&arena);
    uint64_t *number = (uint64_t *)brew_arena_alloc(&arena, sizeof(*number));
    assert(number && (uintptr_t)number % _Alignof(max_align_t) == 0);
    *number = UINT64_C(0x123456789abcdef0);

    unsigned char *aligned = (unsigned char *)brew_arena_alloc_aligned(&arena, 17, 16);
    assert(aligned && (uintptr_t)aligned % 16 == 0);
    void *zeroed = brew_arena_calloc(&arena, 8, sizeof(uint32_t));
    assert(zeroed);
    for (size_t i = 0; i < 8 * sizeof(uint32_t); ++i)
        assert(((unsigned char *)zeroed)[i] == 0);

    size_t used = arena.used;
    assert(!brew_arena_alloc_aligned(&arena, 1, 3));
    assert(arena.used == used);
    assert(!brew_arena_calloc(&arena, SIZE_MAX, 2));
    assert(arena.used == used);
    assert(!brew_arena_alloc(&arena, sizeof(storage.bytes)));
    assert(arena.used == used);

    assert(brew_arena_rewind(&arena, start));
    assert(arena.used == 0);
    assert(!brew_arena_rewind(&arena, 1));
    brew_arena_reset(&arena);
    assert(brew_arena_remaining(&arena) == sizeof(storage.bytes));

    BrewArena empty;
    assert(brew_arena_init(&empty, NULL, 0));
    assert(!brew_arena_alloc(&empty, 1));
    puts("PASS: fixed-capacity arena alignment, bounds, overflow, zeroing, rewind, and reset");
    return 0;
}
