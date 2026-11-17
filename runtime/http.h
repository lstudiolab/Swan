#ifndef SWAN_RUNTIME_HTTP_H
#define SWAN_RUNTIME_HTTP_H
#include "bytes.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define SWAN_HTTP_MAX_HEADERS 64u
#define SWAN_HTTP_HEADER_LIMIT (32u * 1024u)
#define SWAN_HTTP_BODY_LIMIT (8u * 1024u * 1024u)

typedef enum {
    SWAN_HTTP_OK = 0, SWAN_HTTP_INCOMPLETE, SWAN_HTTP_BAD_REQUEST,
    SWAN_HTTP_TOO_LARGE, SWAN_HTTP_UNSUPPORTED
} SwanHttpStatus;

typedef struct {
    const char *name;
    size_t name_length;
    const char *value;
    size_t value_length;
} SwanHttpHeader;

typedef struct {
    const char *method, *target, *path, *query;
    size_t method_length, target_length, path_length, query_length;
    unsigned major_version, minor_version;
    SwanHttpHeader headers[SWAN_HTTP_MAX_HEADERS];
    size_t header_count;
    const unsigned char *body;
    size_t body_length, consumed;
} SwanHttpRequest;

static inline bool swan_http_token_char(unsigned char c) {
    if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
        (c >= 'a' && c <= 'z')) return true;
    switch (c) {
        case '!': case '#': case '$': case '%': case '&': case 39:
        case '*': case '+': case '-': case '.': case '^': case '_':
        case 96: case '|': case '~': return true;
        default: return false;
    }
}

static inline bool swan_http_equal_ci(const char *a, size_t n, const char *b) {
    size_t m = strlen(b);
    if (n != m) return false;
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

static inline size_t swan_http_crlf(const unsigned char *p, size_t n, size_t start) {
    for (size_t i = start; i + 1 < n; i++)
        if (p[i] == 13 && p[i + 1] == 10) return i;
    return SIZE_MAX;
}

static inline SwanHttpStatus swan_http_parse_request(
    const void *input, size_t length, size_t header_limit, size_t body_limit,
    SwanHttpRequest *r) {
    if (!input || !r) return SWAN_HTTP_BAD_REQUEST;
    memset(r, 0, sizeof(*r));
    const unsigned char *p = (const unsigned char *)input;
    if (!header_limit) header_limit = SWAN_HTTP_HEADER_LIMIT;
    if (!body_limit) body_limit = SWAN_HTTP_BODY_LIMIT;
    if (length > header_limit && length - header_limit > body_limit) return SWAN_HTTP_TOO_LARGE;

    size_t line = swan_http_crlf(p, length, 0);
    if (line == SIZE_MAX) return length >= header_limit ? SWAN_HTTP_TOO_LARGE : SWAN_HTTP_INCOMPLETE;
    if (!line || line > 8192) return SWAN_HTTP_BAD_REQUEST;
    size_t a = 0;
    while (a < line && p[a] != ' ') {
        if (!swan_http_token_char(p[a])) return SWAN_HTTP_BAD_REQUEST;
        a++;
    }
    if (!a || a == line) return SWAN_HTTP_BAD_REQUEST;
    size_t b = a + 1;
    while (b < line && p[b] != ' ') {
        if (p[b] <= 32 || p[b] == 127) return SWAN_HTTP_BAD_REQUEST;
        b++;
    }
    if (b == a + 1 || b == line) return SWAN_HTTP_BAD_REQUEST;
    size_t v = b + 1;
    if (line - v != 8 || memcmp(p + v, "HTTP/1.", 7) ||
        (p[v + 7] != '0' && p[v + 7] != '1')) return SWAN_HTTP_UNSUPPORTED;
    r->method = (const char *)p; r->method_length = a;
    r->target = (const char *)p + a + 1; r->target_length = b - a - 1;
    r->major_version = 1; r->minor_version = (unsigned)(p[v + 7] - '0');
    size_t q = 0;
    while (q < r->target_length && r->target[q] != '?') q++;
    r->path = r->target; r->path_length = q;
    if (q < r->target_length) {
        r->query = r->target + q + 1;
        r->query_length = r->target_length - q - 1;
    }
    if (!r->target_length || r->target[0] != '/') return SWAN_HTTP_UNSUPPORTED;

    size_t cursor = line + 2, header_end = SIZE_MAX;
    bool has_length = false, has_transfer = false;
    uint64_t content_length = 0;
    while (cursor < length) {
        size_t end = swan_http_crlf(p, length, cursor);
        if (end == SIZE_MAX) return length >= header_limit ? SWAN_HTTP_TOO_LARGE : SWAN_HTTP_INCOMPLETE;
        if (end - cursor > 8192 || end + 2 > header_limit) return SWAN_HTTP_TOO_LARGE;
        if (end == cursor) { header_end = end + 2; break; }
        if (r->header_count == SWAN_HTTP_MAX_HEADERS || p[cursor] == ' ' || p[cursor] == 9)
            return SWAN_HTTP_TOO_LARGE;
        size_t colon = cursor;
        while (colon < end && p[colon] != ':') {
            if (!swan_http_token_char(p[colon])) return SWAN_HTTP_BAD_REQUEST;
            colon++;
        }
        if (colon == cursor || colon == end) return SWAN_HTTP_BAD_REQUEST;
        size_t first = colon + 1, last = end;
        while (first < last && (p[first] == ' ' || p[first] == 9)) first++;
        while (last > first && (p[last - 1] == ' ' || p[last - 1] == 9)) last--;
        for (size_t i = first; i < last; i++)
            if ((p[i] < 32 && p[i] != 9) || p[i] == 127) return SWAN_HTTP_BAD_REQUEST;
        SwanHttpHeader *h = &r->headers[r->header_count++];
        h->name = (const char *)p + cursor; h->name_length = colon - cursor;
        h->value = (const char *)p + first; h->value_length = last - first;
        if (swan_http_equal_ci(h->name, h->name_length, "content-length")) {
            if (!h->value_length) return SWAN_HTTP_BAD_REQUEST;
            uint64_t parsed = 0;
            for (size_t i = 0; i < h->value_length; i++) {
                unsigned char c = (unsigned char)h->value[i];
                if (c < '0' || c > '9') return SWAN_HTTP_BAD_REQUEST;
                unsigned d = c - '0';
                if (parsed > (UINT64_MAX - d) / 10) return SWAN_HTTP_TOO_LARGE;
                parsed = parsed * 10 + d;
            }
            if (has_length && parsed != content_length) return SWAN_HTTP_BAD_REQUEST;
            has_length = true; content_length = parsed;
        } else if (swan_http_equal_ci(h->name, h->name_length, "transfer-encoding")) {
            has_transfer = true;
        }
        cursor = end + 2;
    }
    if (header_end == SIZE_MAX) return SWAN_HTTP_INCOMPLETE;
    if (has_length && has_transfer) return SWAN_HTTP_BAD_REQUEST;
    if (has_transfer) return SWAN_HTTP_UNSUPPORTED;
    if (content_length > body_limit) return SWAN_HTTP_TOO_LARGE;
    if (r->minor_version == 1) {
        bool host = false;
        for (size_t i = 0; i < r->header_count; i++)
            if (swan_http_equal_ci(r->headers[i].name, r->headers[i].name_length, "host")) host = true;
        if (!host) return SWAN_HTTP_BAD_REQUEST;
    }
    if (content_length > length - header_end) return SWAN_HTTP_INCOMPLETE;
    r->body = p + header_end; r->body_length = (size_t)content_length;
    r->consumed = header_end + (size_t)content_length;
    return SWAN_HTTP_OK;
}

static inline const SwanHttpHeader *swan_http_header(const SwanHttpRequest *r, const char *name) {
    if (!r || !name) return NULL;
    for (size_t i = 0; i < r->header_count; i++)
        if (swan_http_equal_ci(r->headers[i].name, r->headers[i].name_length, name))
            return &r->headers[i];
    return NULL;
}

static inline bool swan_http_safe_field(const char *value) {
    if (!value || !*value) return false;
    for (const unsigned char *p = (const unsigned char *)value; *p; p++)
        if (*p < 32 || *p == 127) return false;
    return true;
}

static inline bool swan_http_response(SwanByteBuffer *out, unsigned status,
    const char *reason, const char *content_type, const void *body, size_t body_length) {
    if (!out || status < 100 || status > 599 ||
        !swan_http_safe_field(reason) || !swan_http_safe_field(content_type) ||
        (!body && body_length)) return false;
    char header[512];
    int n = snprintf(header, sizeof(header),
        "HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",
        status, reason, content_type, body_length);
    if (n < 0 || (size_t)n >= sizeof(header)) return false;
    out->length = 0;
    return swan_bytes_append(out, header, (size_t)n) &&
           swan_bytes_append(out, body, body_length);
}
#endif
