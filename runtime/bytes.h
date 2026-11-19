#ifndef BREW_RUNTIME_BYTES_H
#define BREW_RUNTIME_BYTES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
    bool owns_data;
} BrewByteBuffer;

static inline void brew_bytes_init(BrewByteBuffer *buffer) {
    if (buffer) memset(buffer, 0, sizeof(*buffer));
}

static inline void brew_bytes_destroy(BrewByteBuffer *buffer) {
    if (!buffer) return;
    if (buffer->owns_data) free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

/* Attach caller-owned storage. The caller must keep it alive until the
 * buffer is destroyed or reinitialized. Capacity exhaustion never allocates. */
static inline bool brew_bytes_use_storage(BrewByteBuffer *buffer,
                                           unsigned char *storage,
                                           size_t capacity) {
    if (!buffer || !storage || !capacity || buffer->length != 0) return false;
    if (buffer->owns_data) free(buffer->data);
    buffer->data = storage;
    buffer->capacity = capacity;
    buffer->owns_data = false;
    return true;
}

static inline bool brew_bytes_reserve(BrewByteBuffer *buffer, size_t capacity) {
    if (!buffer) return false;
    if (capacity <= buffer->capacity) return true;
    if (buffer->data && !buffer->owns_data) return false;
    size_t next = buffer->capacity ? buffer->capacity : 64;
    while (next < capacity) {
        if (next > SIZE_MAX / 2) { next = capacity; break; }
        next *= 2;
    }
    void *memory = realloc(buffer->data, next);
    if (!memory) return false;
    buffer->data = (unsigned char *)memory;
    buffer->capacity = next;
    buffer->owns_data = true;
    return true;
}

static inline bool brew_bytes_resize(BrewByteBuffer *buffer, size_t length,
                                      unsigned char fill) {
    if (!buffer || !brew_bytes_reserve(buffer, length)) return false;
    if (length > buffer->length)
        memset(buffer->data + buffer->length, fill, length - buffer->length);
    buffer->length = length;
    return true;
}

static inline bool brew_bytes_append(BrewByteBuffer *buffer,
                                      const void *data, size_t length) {
    if (!buffer || (!data && length) ||
        length > SIZE_MAX - buffer->length) return false;
    size_t old_length = buffer->length;
    bool aliases = false;
    size_t source_offset = 0;
    if (length && buffer->data) {
        uintptr_t base = (uintptr_t)buffer->data;
        uintptr_t source = (uintptr_t)data;
        if (source >= base && source - base <= old_length) {
            source_offset = (size_t)(source - base);
            if (length > old_length - source_offset) return false;
            aliases = true;
        }
    }
    if (!brew_bytes_reserve(buffer, old_length + length)) return false;
    if (aliases) data = buffer->data + source_offset;
    if (length) memmove(buffer->data + old_length, data, length);
    buffer->length += length;
    return true;
}

static inline bool brew_bytes_append_byte(BrewByteBuffer *buffer,
                                           unsigned char value) {
    return brew_bytes_append(buffer, &value, 1);
}

static inline void brew_bytes_clear(BrewByteBuffer *buffer) {
    if (buffer) buffer->length = 0;
}

#ifdef __cplusplus
}
#endif
#endif
