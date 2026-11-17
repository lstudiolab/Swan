#ifndef SWAN_RUNTIME_JSON_ABI_H
#define SWAN_RUNTIME_JSON_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Handles are opaque to Swan and travel as machine-word C ABI values. */
void *swan_json_create(int64_t initial_token_capacity);
int32_t swan_json_parse_text(void *document, const char *input);
int64_t swan_json_input_length(void *document);
int64_t swan_json_get_i64(void *document, const char *key, int64_t fallback);
int32_t swan_json_has_key(void *document, const char *key);

/* Returned text is borrowed from the handle and remains valid until close. */
const char *swan_json_compact(void *document);
int64_t swan_json_compact_length(void *document);
const char *swan_json_pretty(void *document);
const char *swan_json_error(void *document);
void swan_json_close(void *document);

#ifdef __cplusplus
}
#endif
#endif
