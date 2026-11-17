#ifndef SWAN_RUNTIME_BYTES_H
#define SWAN_RUNTIME_BYTES_H

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
} SwanByteBuffer;

static inline void swan_bytes_init(SwanByteBuffer *buffer) {
    if (buffer) memset(buffer, 0, sizeof(*buffer));
}

static inline void swan_bytes_destroy(SwanByteBuffer *buffer) {
    if (!buffer) return;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

static inline bool swan_bytes_reserve(SwanByteBuffer *buffer, size_t capacity) {
    if (!buffer) return false;
    if (capacity <= buffer->capacity) return true;
    size_t next = buffer->capacity ? buffer->capacity : 64;
    while (next < capacity) {
        if (next > SIZE_MAX / 2) { next = capacity; break; }
        next *= 2;
    }
    void *memory = realloc(buffer->data, next);
    if (!memory) return false;
    buffer->data = (unsigned char *)memory;
    buffer->capacity = next;
    return true;
}

static inline bool swan_bytes_resize(SwanByteBuffer *buffer, size_t length,
                                      unsigned char fill) {
    if (!buffer || !swan_bytes_reserve(buffer, length)) return false;
    if (length > buffer->length)
        memset(buffer->data + buffer->length, fill, length - buffer->length);
    buffer->length = length;
    return true;
}

static inline bool swan_bytes_append(SwanByteBuffer *buffer,
                                      const void *data, size_t length) {
    if (!buffer || (!data && length) ||
        length > SIZE_MAX - buffer->length) return false;
    size_t old_length = buffer->length;
    if (!swan_bytes_reserve(buffer, old_length + length)) return false;
    if (length) memmove(buffer->data + old_length, data, length);
    buffer->length += length;
    return true;
}

static inline bool swan_bytes_append_byte(SwanByteBuffer *buffer,
                                           unsigned char value) {
    return swan_bytes_append(buffer, &value, 1);
}

static inline void swan_bytes_clear(SwanByteBuffer *buffer) {
    if (buffer) buffer->length = 0;
}

#ifdef __cplusplus
}
#endif
#endif
