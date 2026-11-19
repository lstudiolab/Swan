#include "json_abi.h"
#include "json.h"
#include <stdlib.h>
#include <string.h>

typedef struct BrewJsonHandle BrewJsonHandle;

struct BrewJsonHandle {
    BrewJsonDocument document;
    char *input;
    BrewJsonBuffer compact;
    BrewJsonBuffer pretty;
    char error[128];
};

void *brew_json_create(int64_t initial_token_capacity) {
    BrewJsonHandle *handle = (BrewJsonHandle *)calloc(1, sizeof(*handle));
    if (!handle) return NULL;
    brew_json_document_init(&handle->document);
    if (initial_token_capacity > 0 &&
        (uint64_t)initial_token_capacity <= handle->document.limits.max_tokens) {
        size_t capacity = (size_t)initial_token_capacity;
        if (capacity > SIZE_MAX / sizeof(*handle->document.tokens)) {
            brew_json_document_destroy(&handle->document);
            free(handle);
            return NULL;
        }
        handle->document.tokens = (BrewJsonToken *)malloc(
            capacity * sizeof(*handle->document.tokens));
        if (!handle->document.tokens) {
            brew_json_document_destroy(&handle->document);
            free(handle);
            return NULL;
        }
        handle->document.token_capacity = capacity;
    }
    return handle;
}

int32_t brew_json_parse_text(void *document, const char *input) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
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
    bool ok = brew_json_parse_reuse(&handle->document, handle->input, length);
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

int64_t brew_json_input_length(void *document) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
    return handle && !handle->error[0] ? (int64_t)handle->document.input_length : -1;
}

int64_t brew_json_get_i64(void *document, const char *key,
                          int64_t fallback) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
    if (!handle || !handle->document.token_count || !key ||
        handle->error[0]) return fallback;
    size_t token = brew_json_object_get(&handle->document, 0, key);
    int64_t value;
    return token != BREW_JSON_NONE &&
           brew_json_as_i64(&handle->document, token, &value) ? value : fallback;
}

int32_t brew_json_has_key(void *document, const char *key) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
    if (!handle || !handle->document.token_count || !key ||
        handle->error[0]) return 0;
    return brew_json_object_get(&handle->document, 0, key) != BREW_JSON_NONE;
}

const char *brew_json_compact(void *document) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
    if (!handle || handle->error[0] ||
        !brew_json_encode(&handle->document, &handle->compact, false))
        return NULL;
    return handle->compact.data;
}

int64_t brew_json_compact_length(void *document) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
    const char *text = brew_json_compact(document);
    return text ? (int64_t)handle->compact.length : -1;
}

const char *brew_json_pretty(void *document) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
    if (!handle || handle->error[0] ||
        !brew_json_encode(&handle->document, &handle->pretty, true))
        return NULL;
    return handle->pretty.data;
}

const char *brew_json_error(void *document) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
    if (!handle) return "invalid JSON handle";
    return handle->error;
}

void brew_json_close(void *document) {
    BrewJsonHandle *handle = (BrewJsonHandle *)document;
    if (!handle) return;
    brew_json_document_destroy(&handle->document);
    brew_json_buffer_destroy(&handle->compact);
    brew_json_buffer_destroy(&handle->pretty);
    free(handle->input);
    free(handle);
}
