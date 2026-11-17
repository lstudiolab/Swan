#ifndef SWAN_RUNTIME_JSON_WRITER_H
#define SWAN_RUNTIME_JSON_WRITER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Streaming JSON builder. All strings are NUL-terminated UTF-8 C strings. */
void *swan_json_writer_create(int64_t max_depth);
int32_t swan_json_writer_begin_object(void *writer);
int32_t swan_json_writer_end_object(void *writer);
int32_t swan_json_writer_begin_array(void *writer);
int32_t swan_json_writer_end_array(void *writer);
int32_t swan_json_writer_key(void *writer, const char *key);
int32_t swan_json_writer_string(void *writer, const char *value);
int32_t swan_json_writer_i64(void *writer, int64_t value);
int32_t swan_json_writer_bool(void *writer, int32_t value);
int32_t swan_json_writer_null(void *writer);
const char *swan_json_writer_finish(void *writer);
int64_t swan_json_writer_length(void *writer);
const char *swan_json_writer_error(void *writer);
void swan_json_writer_destroy(void *writer);

#ifdef __cplusplus
}
#endif
#endif
