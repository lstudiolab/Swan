#ifndef SWAN_RUNTIME_JSON_H
#define SWAN_RUNTIME_JSON_H

/*
 * Swan JSON runtime, C11.
 *
 * The parser builds a compact token tree over borrowed input bytes. String and
 * number tokens retain source spans; parsing does not allocate per character.
 * The document must not outlive its input buffer.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SWAN_JSON_OBJECT, SWAN_JSON_ARRAY, SWAN_JSON_STRING,
    SWAN_JSON_NUMBER, SWAN_JSON_TRUE, SWAN_JSON_FALSE, SWAN_JSON_NULL
} SwanJsonKind;

typedef struct {
    SwanJsonKind kind;
    size_t start, end;
    size_t first_child, last_child, next_sibling, child_count;
} SwanJsonToken;

typedef struct {
    size_t max_depth;
    size_t max_tokens;
    size_t max_input_bytes;
} SwanJsonLimits;

typedef struct {
    const unsigned char *input;
    size_t input_length;
    SwanJsonToken *tokens;
    size_t token_count, token_capacity;
    SwanJsonLimits limits;
    size_t error_offset;
    char error[128];
} SwanJsonDocument;

typedef struct {
    char *data;
    size_t length, capacity;
} SwanJsonBuffer;

#define SWAN_JSON_NONE SIZE_MAX

static inline void swan_json_document_init(SwanJsonDocument *doc) {
    if (!doc) return;
    memset(doc, 0, sizeof(*doc));
    doc->limits.max_depth = 128;
    doc->limits.max_tokens = 1000000;
    doc->limits.max_input_bytes = 64u * 1024u * 1024u;
}

static inline void swan_json_document_destroy(SwanJsonDocument *doc) {
    if (!doc) return;
    free(doc->tokens);
    memset(doc, 0, sizeof(*doc));
}

static inline bool swan_json_fail(SwanJsonDocument *doc, size_t offset,
                                  const char *message) {
    if (doc) {
        doc->error_offset = offset;
        snprintf(doc->error, sizeof(doc->error), "%s", message);
    }
    return false;
}

static inline bool swan_json_reserve_tokens(SwanJsonDocument *doc) {
    if (doc->token_count >= doc->limits.max_tokens)
        return swan_json_fail(doc, doc->input_length, "token limit exceeded");
    if (doc->token_count < doc->token_capacity) return true;
    size_t next = doc->token_capacity ? doc->token_capacity * 2 : 64;
    if (next < doc->token_capacity || next > doc->limits.max_tokens)
        next = doc->limits.max_tokens;
    if (next <= doc->token_capacity ||
        next > SIZE_MAX / sizeof(*doc->tokens))
        return swan_json_fail(doc, doc->input_length, "token allocation overflow");
    void *p = realloc(doc->tokens, next * sizeof(*doc->tokens));
    if (!p) return swan_json_fail(doc, doc->input_length, "out of memory");
    doc->tokens = (SwanJsonToken *)p;
    doc->token_capacity = next;
    return true;
}

static inline size_t swan_json_add_token(SwanJsonDocument *doc,
                                          SwanJsonKind kind, size_t start) {
    if (!swan_json_reserve_tokens(doc)) return SWAN_JSON_NONE;
    size_t id = doc->token_count++;
    doc->tokens[id] = (SwanJsonToken){
        kind, start, start, SWAN_JSON_NONE, SWAN_JSON_NONE,
        SWAN_JSON_NONE, 0
    };
    return id;
}

static inline void swan_json_add_child(SwanJsonDocument *doc,
                                        size_t parent, size_t child) {
    SwanJsonToken *p = &doc->tokens[parent];
    if (p->last_child == SWAN_JSON_NONE) p->first_child = child;
    else doc->tokens[p->last_child].next_sibling = child;
    p->last_child = child;
    p->child_count++;
}

static inline bool swan_json_utf8(const unsigned char *s, size_t n,
                                   size_t *width) {
    if (!n) return false;
    unsigned char a = s[0];
    if (a < 0x80) { *width = 1; return true; }
    if (a >= 0xC2 && a <= 0xDF) {
        if (n < 2 || (s[1] & 0xC0) != 0x80) return false;
        *width = 2; return true;
    }
    if (a >= 0xE0 && a <= 0xEF) {
        if (n < 3 || (s[1] & 0xC0) != 0x80 ||
            (s[2] & 0xC0) != 0x80) return false;
        if ((a == 0xE0 && s[1] < 0xA0) ||
            (a == 0xED && s[1] >= 0xA0)) return false;
        *width = 3; return true;
    }
    if (a >= 0xF0 && a <= 0xF4) {
        if (n < 4 || (s[1] & 0xC0) != 0x80 ||
            (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80)
            return false;
        if ((a == 0xF0 && s[1] < 0x90) ||
            (a == 0xF4 && s[1] >= 0x90)) return false;
        *width = 4; return true;
    }
    return false;
}

typedef struct {
    SwanJsonDocument *doc;
    size_t pos;
} SwanJsonParser;

static inline void swan_json_ws(SwanJsonParser *p) {
    const unsigned char *s = p->doc->input;
    while (p->pos < p->doc->input_length &&
           (s[p->pos] == ' ' || s[p->pos] == '\t' ||
            s[p->pos] == '\r' || s[p->pos] == '\n')) p->pos++;
}

static inline int swan_json_hex(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static inline bool swan_json_u16(SwanJsonParser *p, uint32_t *value) {
    if (p->doc->input_length - p->pos < 4) return false;
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; i++) {
        int h = swan_json_hex(p->doc->input[p->pos++]);
        if (h < 0) return false;
        v = (v << 4) | (uint32_t)h;
    }
    *value = v;
    return true;
}

static inline bool swan_json_string_scan(SwanJsonParser *p) {
    const unsigned char *s = p->doc->input;
    size_t n = p->doc->input_length;
    if (p->pos >= n || s[p->pos] != '"') return false;
    p->pos++;
    while (p->pos < n) {
        unsigned char c = s[p->pos++];
        if (c == '"') return true;
        if (c < 0x20) return false;
        if (c == '\\') {
            if (p->pos >= n) return false;
            c = s[p->pos++];
            if (c == '"' || c == '\\' || c == '/' ||
                c == 'b' || c == 'f' || c == 'n' ||
                c == 'r' || c == 't') continue;
            if (c != 'u') return false;
            uint32_t u;
            if (!swan_json_u16(p, &u)) return false;
            if (u >= 0xD800 && u <= 0xDBFF) {
                if (n - p->pos < 6 || s[p->pos] != '\\' ||
                    s[p->pos + 1] != 'u') return false;
                p->pos += 2;
                uint32_t low;
                if (!swan_json_u16(p, &low) ||
                    low < 0xDC00 || low > 0xDFFF) return false;
            } else if (u >= 0xDC00 && u <= 0xDFFF) return false;
            continue;
        }
        if (c >= 0x80) {
            size_t width;
            p->pos--;
            if (!swan_json_utf8(s + p->pos, n - p->pos, &width))
                return false;
            p->pos += width;
        }
    }
    return false;
}

static inline bool swan_json_parse_value(SwanJsonParser *p, size_t depth,
                                          size_t *result);

static inline bool swan_json_parse_compound(SwanJsonParser *p, size_t depth,
                                             size_t *result, bool object) {
    SwanJsonDocument *d = p->doc;
    const unsigned char *s = d->input;
    size_t id = swan_json_add_token(d, object ? SWAN_JSON_OBJECT : SWAN_JSON_ARRAY,
                                    p->pos);
    if (id == SWAN_JSON_NONE) return false;
    unsigned char close = object ? '}' : ']';
    p->pos++;
    swan_json_ws(p);
    if (p->pos < d->input_length && s[p->pos] == close) {
        p->pos++;
        d->tokens[id].end = p->pos;
        *result = id;
        return true;
    }
    for (;;) {
        if (object) {
            size_t key_start = p->pos;
            if (!swan_json_string_scan(p))
                return swan_json_fail(d, p->pos, "invalid object key string");
            size_t key = swan_json_add_token(d, SWAN_JSON_STRING, key_start);
            if (key == SWAN_JSON_NONE) return false;
            d->tokens[key].end = p->pos;
            swan_json_add_child(d, id, key);
            swan_json_ws(p);
            if (p->pos >= d->input_length || s[p->pos++] != ':')
                return swan_json_fail(d, p->pos, "expected ':' after object key");
            swan_json_ws(p);
        }
        size_t child;
        if (!swan_json_parse_value(p, depth + 1, &child)) return false;
        swan_json_add_child(d, id, child);
        swan_json_ws(p);
        if (p->pos >= d->input_length)
            return swan_json_fail(d, p->pos, "unterminated array or object");
        unsigned char c = s[p->pos++];
        if (c == close) break;
        if (c != ',')
            return swan_json_fail(d, p->pos - 1, "expected ',' or closing delimiter");
        swan_json_ws(p);
    }
    if (object && (d->tokens[id].child_count & 1u))
        return swan_json_fail(d, p->pos, "object key has no value");
    d->tokens[id].end = p->pos;
    *result = id;
    return true;
}

static inline bool swan_json_number_scan(SwanJsonParser *p) {
    const unsigned char *s = p->doc->input;
    size_t n = p->doc->input_length, i = p->pos;
    if (i < n && s[i] == '-') i++;
    if (i >= n) return false;
    if (s[i] == '0') {
        i++;
        if (i < n && s[i] >= '0' && s[i] <= '9') return false;
    } else if (s[i] >= '1' && s[i] <= '9') {
        do { i++; } while (i < n && s[i] >= '0' && s[i] <= '9');
    } else return false;
    if (i < n && s[i] == '.') {
        i++;
        if (i >= n || s[i] < '0' || s[i] > '9') return false;
        do { i++; } while (i < n && s[i] >= '0' && s[i] <= '9');
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        i++;
        if (i < n && (s[i] == '+' || s[i] == '-')) i++;
        if (i >= n || s[i] < '0' || s[i] > '9') return false;
        do { i++; } while (i < n && s[i] >= '0' && s[i] <= '9');
    }
    p->pos = i;
    return true;
}

static inline bool swan_json_parse_value(SwanJsonParser *p, size_t depth,
                                          size_t *result) {
    SwanJsonDocument *d = p->doc;
    const unsigned char *s = d->input;
    swan_json_ws(p);
    if (depth > d->limits.max_depth)
        return swan_json_fail(d, p->pos, "maximum nesting depth exceeded");
    if (p->pos >= d->input_length)
        return swan_json_fail(d, p->pos, "expected JSON value");
    size_t start = p->pos, id;
    unsigned char c = s[p->pos];
    if (c == '{') return swan_json_parse_compound(p, depth, result, true);
    if (c == '[') return swan_json_parse_compound(p, depth, result, false);
    if (c == '"') {
        if (!swan_json_string_scan(p))
            return swan_json_fail(d, p->pos, "invalid JSON string or Unicode escape");
        id = swan_json_add_token(d, SWAN_JSON_STRING, start);
        if (id == SWAN_JSON_NONE) return false;
        d->tokens[id].end = p->pos;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        if (!swan_json_number_scan(p))
            return swan_json_fail(d, p->pos, "invalid JSON number");
        id = swan_json_add_token(d, SWAN_JSON_NUMBER, start);
        if (id == SWAN_JSON_NONE) return false;
        d->tokens[id].end = p->pos;
    } else {
        SwanJsonKind kind;
        const char *word;
        size_t length;
        if (c == 't') { kind = SWAN_JSON_TRUE; word = "true"; length = 4; }
        else if (c == 'f') { kind = SWAN_JSON_FALSE; word = "false"; length = 5; }
        else if (c == 'n') { kind = SWAN_JSON_NULL; word = "null"; length = 4; }
        else return swan_json_fail(d, p->pos, "unexpected character");
        if (length > d->input_length - p->pos ||
            memcmp(s + p->pos, word, length) != 0)
            return swan_json_fail(d, p->pos, "invalid JSON literal");
        p->pos += length;
        id = swan_json_add_token(d, kind, start);
        if (id == SWAN_JSON_NONE) return false;
        d->tokens[id].end = p->pos;
    }
    *result = id;
    return true;
}

/* Input is borrowed; it must remain alive until document_destroy or reparse. */
static inline bool swan_json_parse_reuse(SwanJsonDocument *doc,
                                          const void *input, size_t length) {
    if (!doc || (!input && length)) return false;
    doc->input = (const unsigned char *)input;
    doc->input_length = length;
    doc->token_count = 0;
    doc->error_offset = 0;
    doc->error[0] = 0;
    if (!doc->limits.max_depth) doc->limits.max_depth = 128;
    if (!doc->limits.max_tokens) doc->limits.max_tokens = 1000000;
    if (!doc->limits.max_input_bytes) doc->limits.max_input_bytes = 64u * 1024u * 1024u;
    if (length > doc->limits.max_input_bytes)
        return swan_json_fail(doc, 0, "input size limit exceeded");
    SwanJsonParser parser = { doc, 0 };
    size_t root;
    if (!swan_json_parse_value(&parser, 0, &root)) return false;
    swan_json_ws(&parser);
    if (parser.pos != length)
        return swan_json_fail(doc, parser.pos, "trailing data after JSON value");
    return root == 0;
}

/* Reset and parse while retaining allocated token capacity. */
static inline bool swan_json_parse(SwanJsonDocument *doc,
                                    const void *input, size_t length) {
    return swan_json_parse_reuse(doc, input, length);
}
static inline bool swan_json_token_text(const SwanJsonDocument *doc,
                                         size_t token, const char **text,
                                         size_t *length) {
    if (!doc || token >= doc->token_count) return false;
    const SwanJsonToken *t = &doc->tokens[token];
    if (t->kind != SWAN_JSON_STRING && t->kind != SWAN_JSON_NUMBER)
        return false;
    size_t start = t->start, end = t->end;
    if (t->kind == SWAN_JSON_STRING) { start++; end--; }
    if (text) *text = (const char *)doc->input + start;
    if (length) *length = end - start;
    return true;
}

static inline bool swan_json_key_equals(const SwanJsonDocument *doc,
                                         size_t token, const char *key) {
    const char *text;
    size_t length;
    if (!key || !swan_json_token_text(doc, token, &text, &length)) return false;
    size_t i = 0, j = 0, key_length = strlen(key);
    while (i < length) {
        uint32_t cp = (unsigned char)text[i++];
        if (cp == '\\') {
            if (i >= length) return false;
            unsigned char escaped = (unsigned char)text[i++];
            switch (escaped) {
                case '"': cp = '"'; break;
                case '\\': cp = '\\'; break;
                case '/': cp = '/'; break;
                case 'b': cp = '\b'; break;
                case 'f': cp = '\f'; break;
                case 'n': cp = '\n'; break;
                case 'r': cp = '\r'; break;
                case 't': cp = '\t'; break;
                case 'u': {
                    if (length - i < 4) return false;
                    cp = 0;
                    for (unsigned n = 0; n < 4; n++) {
                        int h = swan_json_hex((unsigned char)text[i++]);
                        if (h < 0) return false;
                        cp = (cp << 4) | (uint32_t)h;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (length - i < 6 || text[i++] != '\\' ||
                            text[i++] != 'u') return false;
                        uint32_t low = 0;
                        for (unsigned n = 0; n < 4; n++) {
                            int h = swan_json_hex((unsigned char)text[i++]);
                            if (h < 0) return false;
                            low = (low << 4) | (uint32_t)h;
                        }
                        if (low < 0xDC00 || low > 0xDFFF) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) return false;
                    break;
                }
                default: return false;
            }
        }
        unsigned char encoded[4];
        size_t width;
        if (cp <= 0x7F) { encoded[0] = (unsigned char)cp; width = 1; }
        else if (cp <= 0x7FF) {
            encoded[0] = (unsigned char)(0xC0 | (cp >> 6));
            encoded[1] = (unsigned char)(0x80 | (cp & 0x3F)); width = 2;
        } else if (cp <= 0xFFFF) {
            encoded[0] = (unsigned char)(0xE0 | (cp >> 12));
            encoded[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            encoded[2] = (unsigned char)(0x80 | (cp & 0x3F)); width = 3;
        } else if (cp <= 0x10FFFF) {
            encoded[0] = (unsigned char)(0xF0 | (cp >> 18));
            encoded[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
            encoded[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            encoded[3] = (unsigned char)(0x80 | (cp & 0x3F)); width = 4;
        } else return false;
        if (width > key_length - j || memcmp(key + j, encoded, width)) return false;
        j += width;
    }
    return j == key_length;
}
static inline size_t swan_json_object_get(const SwanJsonDocument *doc,
                                           size_t object, const char *key) {
    if (!doc || object >= doc->token_count ||
        doc->tokens[object].kind != SWAN_JSON_OBJECT) return SWAN_JSON_NONE;
    size_t k = doc->tokens[object].first_child;
    while (k != SWAN_JSON_NONE) {
        size_t value = doc->tokens[k].next_sibling;
        if (value == SWAN_JSON_NONE) return SWAN_JSON_NONE;
        if (swan_json_key_equals(doc, k, key)) return value;
        k = doc->tokens[value].next_sibling;
    }
    return SWAN_JSON_NONE;
}

static inline bool swan_json_as_i64(const SwanJsonDocument *doc,
                                     size_t token, int64_t *value) {
    const char *text;
    size_t length;
    if (!value || !swan_json_token_text(doc, token, &text, &length) ||
        !length || length >= 64) return false;
    char tmp[64];
    memcpy(tmp, text, length); tmp[length] = 0;
    if (strchr(tmp, '.') || strchr(tmp, 'e') || strchr(tmp, 'E')) return false;
    errno = 0;
    char *end = NULL;
    long long parsed = strtoll(tmp, &end, 10);
    if (errno == ERANGE || end != tmp + length) return false;
    *value = (int64_t)parsed;
    return true;
}

static inline bool swan_json_buffer_reserve(SwanJsonBuffer *b, size_t extra) {
    if (!b || b->length == SIZE_MAX || extra > SIZE_MAX - b->length - 1) return false;
    size_t need = b->length + extra + 1;
    if (need <= b->capacity) return true;
    size_t cap = b->capacity ? b->capacity : 256;
    while (cap < need) {
        if (cap > SIZE_MAX / 2) { cap = need; break; }
        cap *= 2;
    }
    char *p = (char *)realloc(b->data, cap);
    if (!p) return false;
    b->data = p; b->capacity = cap;
    return true;
}

static inline bool swan_json_buffer_append(SwanJsonBuffer *b,
                                            const void *data, size_t length) {
    if (!swan_json_buffer_reserve(b, length)) return false;
    if (length) memcpy(b->data + b->length, data, length);
    b->length += length;
    b->data[b->length] = 0;
    return true;
}

static inline bool swan_json_encode_token(const SwanJsonDocument *doc,
                                           size_t id, SwanJsonBuffer *out,
                                           bool pretty, size_t depth) {
    if (id >= doc->token_count) return false;
    const SwanJsonToken *t = &doc->tokens[id];
    if (t->kind == SWAN_JSON_STRING || t->kind == SWAN_JSON_NUMBER ||
        t->kind == SWAN_JSON_TRUE || t->kind == SWAN_JSON_FALSE ||
        t->kind == SWAN_JSON_NULL)
        return swan_json_buffer_append(out, doc->input + t->start, t->end - t->start);
    bool object = t->kind == SWAN_JSON_OBJECT;
    char open = object ? '{' : '[', close = object ? '}' : ']';
    if (!swan_json_buffer_append(out, &open, 1)) return false;
    size_t child = t->first_child, index = 0;
    while (child != SWAN_JSON_NONE) {
        if (index && !swan_json_buffer_append(out, ",", 1)) return false;
        if (pretty) {
            if (!swan_json_buffer_append(out, "\n", 1)) return false;
            for (size_t n = 0; n < depth + 1; n++)
                if (!swan_json_buffer_append(out, "  ", 2)) return false;
        }
        if (!swan_json_encode_token(doc, child, out, pretty, depth + 1)) return false;
        child = doc->tokens[child].next_sibling;
        if (object) {
            if (child == SWAN_JSON_NONE ||
                !swan_json_buffer_append(out, pretty ? ": " : ":", pretty ? 2 : 1))
                return false;
            if (!swan_json_encode_token(doc, child, out, pretty, depth + 1)) return false;
            child = doc->tokens[child].next_sibling;
        }
        index++;
    }
    if (pretty && index) {
        if (!swan_json_buffer_append(out, "\n", 1)) return false;
        for (size_t n = 0; n < depth; n++)
            if (!swan_json_buffer_append(out, "  ", 2)) return false;
    }
    return swan_json_buffer_append(out, &close, 1);
}

static inline bool swan_json_encode(const SwanJsonDocument *doc,
                                     SwanJsonBuffer *out, bool pretty) {
    if (!doc || !out || !doc->token_count) return false;
    out->length = 0;
    if (out->data && out->capacity) out->data[0] = 0;
    return swan_json_encode_token(doc, 0, out, pretty, 0);
}

static inline void swan_json_buffer_destroy(SwanJsonBuffer *buffer) {
    if (!buffer) return;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

#ifdef __cplusplus
}
#endif
#endif
