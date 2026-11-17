#ifndef SWAN_RUNTIME_JSON_ABI_H
#define SWAN_RUNTIME_JSON_ABI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SwanJsonHandle SwanJsonHandle;

/* The handle owns a copy of input. Null means invalid input or OOM; a non-null
   handle with a non-empty error reports a parse failure. */
SwanJsonHandle *swan_json_create(int64_t initial_token_capacity);
int32_t swan_json_parse_text(SwanJsonHandle *document, const char *input);
int64_t swan_json_input_length(SwanJsonHandle *document);
int64_t swan_json_get_i64(SwanJsonHandle *document, const char *key,
                          int64_t fallback);
int32_t swan_json_has_key(SwanJsonHandle *document, const char *key);

/* Returned text is borrowed from the handle and remains valid until close. */
const char *swan_json_compact(SwanJsonHandle *document);
int64_t swan_json_compact_length(SwanJsonHandle *document);
const char *swan_json_pretty(SwanJsonHandle *document);
const char *swan_json_error(SwanJsonHandle *document);
void swan_json_close(SwanJsonHandle *document);

#ifdef __cplusplus
}
#endif
#endif
