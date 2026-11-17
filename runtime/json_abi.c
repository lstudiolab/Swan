#include "json_abi.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>

struct SwanJsonHandle {
    SwanJsonDocument document;
    char *input;
    SwanJsonBuffer compact;
    SwanJsonBuffer pretty;
    char error[128];
};

SwanJsonHandle *swan_json_create(void) {
    SwanJsonHandle *handle = (SwanJsonHandle *)calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    swan_json_document_init(&handle->document);
    return handle;
}

int32_t swan_json_parse_text(SwanJsonHandle *handle, const char *input) {
    if (!handle || !input) return 0;
    size_t length = strlen(input);
    if (length == SIZE_MAX) return 0;
    char *next = (char *)realloc(handle->input, length + 1);
    if (!next) {
        snprintf(handle->error, sizeof(handle->error), "out of memory");
        return 0;
    }
    handle->input = next;
    memcpy(handle->input, input, length + 1);
    bool ok = swan_json_parse_reuse(&handle->document, handle->input, length);
    handle->error[0] = 0;
    if (!ok) {
        snprintf(handle->error, sizeof(handle->error), "%s",
                 handle->document.error[0] ? handle->document.error :
                 "invalid JSON");
        return 0;
    }
    handle->compact.length = 0;
    handle->pretty.length = 0;
    return 1;
}

int64_t swan_json_input_length(SwanJsonHandle *handle) {
    return handle && !handle->error[0] ? (int64_t)handle->document.input_length : -1;
}

SwanJsonHandle *swan_json_open(const char *input) {
    SwanJsonHandle *handle = swan_json_create();
    if (!handle) return NULL;
    (void)swan_json_parse_text(handle, input);
    return handle;
}

int64_t swan_json_get_i64(SwanJsonHandle *handle, const char *key,
                          int64_t fallback) {
    if (!handle || !handle->document.token_count || !key ||
        handle->error[0]) return fallback;
    size_t token = swan_json_object_get(&handle->document, 0, key);
    int64_t value;
    return token != SWAN_JSON_NONE &&
           swan_json_as_i64(&handle->document, token, &value) ? value : fallback;
}

int32_t swan_json_has_key(SwanJsonHandle *handle, const char *key) {
    if (!handle || !handle->document.token_count || !key ||
        handle->error[0]) return 0;
    return swan_json_object_get(&handle->document, 0, key) != SWAN_JSON_NONE;
}

const char *swan_json_compact(SwanJsonHandle *handle) {
    if (!handle || handle->error[0] ||
        !swan_json_encode(&handle->document, &handle->compact, false))
        return NULL;
    return handle->compact.data;
}

int64_t swan_json_compact_length(SwanJsonHandle *handle) {
    const char *text = swan_json_compact(handle);
    return text ? (int64_t)handle->compact.length : -1;
}

const char *swan_json_pretty(SwanJsonHandle *handle) {
    if (!handle || handle->error[0] ||
        !swan_json_encode(&handle->document, &handle->pretty, true))
        return NULL;
    return handle->pretty.data;
}

const char *swan_json_error(SwanJsonHandle *handle) {
    if (!handle) return "invalid JSON handle";
    return handle->error;
}

void swan_json_close(SwanJsonHandle *handle) {
    if (!handle) return;
    swan_json_document_destroy(&handle->document);
    swan_json_buffer_destroy(&handle->compact);
    swan_json_buffer_destroy(&handle->pretty);
    free(handle->input);
    free(handle);
}
