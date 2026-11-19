#ifndef BREW_RUNTIME_JSON_H
#define BREW_RUNTIME_JSON_H

/*
 * Brew JSON runtime, C11.
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
    BREW_JSON_OBJECT, BREW_JSON_ARRAY, BREW_JSON_STRING,
    BREW_JSON_NUMBER, BREW_JSON_TRUE, BREW_JSON_FALSE, BREW_JSON_NULL
} BrewJsonKind;

typedef struct {
    BrewJsonKind kind;
    size_t start, end;
    size_t first_child, last_child, next_sibling, child_count;
} BrewJsonToken;

typedef struct {
    size_t max_depth;
    size_t max_tokens;
    size_t max_input_bytes;
} BrewJsonLimits;

typedef struct {
    const unsigned char *input;
    size_t input_length;
    BrewJsonToken *tokens;
    size_t token_count, token_capacity;
    bool owns_tokens;
    BrewJsonLimits limits;
    size_t error_offset;
    char error[128];
} BrewJsonDocument;

typedef struct {
    char *data;
    size_t length, capacity;
} BrewJsonBuffer;

#define BREW_JSON_NONE SIZE_MAX

static inline void brew_json_document_init(BrewJsonDocument *doc) {
    if (!doc) return;
    memset(doc, 0, sizeof(*doc));
    doc->limits.max_depth = 128;
    doc->limits.max_tokens = 1000000;
    doc->limits.max_input_bytes = 64u * 1024u * 1024u;
}

static inline void brew_json_document_destroy(BrewJsonDocument *doc) {
    if (!doc) return;
    if (doc->owns_tokens) free(doc->tokens);
    memset(doc, 0, sizeof(*doc));
}

static inline bool brew_json_fail(BrewJsonDocument *doc, size_t offset,
                                  const char *message) {
    if (doc) {
        doc->error_offset = offset;
        snprintf(doc->error, sizeof(doc->error), "%s", message);
    }
    return false;
}

/* Attach caller-owned token storage for allocation-free parsing. The storage
 * must remain alive until replaced or the document is destroyed. When the
 * capacity is exhausted, parsing fails instead of falling back to heap. */
static inline bool brew_json_document_use_token_storage(
    BrewJsonDocument *doc, BrewJsonToken *storage, size_t capacity) {
    if (!doc || !storage || !capacity || doc->token_count != 0 ||
        capacity < doc->token_capacity)
        return false;
    if (doc->owns_tokens) free(doc->tokens);
    doc->tokens = storage;
    doc->token_capacity = capacity;
    doc->owns_tokens = false;
    return true;
}

/* Reserve token storage before parsing to avoid repeated growth reallocations.
 * Existing tokens are retained; parse_reuse resets the count, not capacity. */
static inline bool brew_json_document_reserve_tokens(BrewJsonDocument *doc,
                                                       size_t requested) {
    if (!doc) return false;
    if (requested > doc->limits.max_tokens)
        return brew_json_fail(doc, doc->input_length, "token limit exceeded");
    if (requested <= doc->token_capacity) return true;
    if (doc->tokens && !doc->owns_tokens)
        return brew_json_fail(doc, doc->input_length, "caller token buffer exhausted");
    if (requested > SIZE_MAX / sizeof(*doc->tokens))
        return brew_json_fail(doc, doc->input_length, "token allocation overflow");
    BrewJsonToken *tokens = (BrewJsonToken *)realloc(
        doc->tokens, requested * sizeof(*doc->tokens));
    if (!tokens)
        return brew_json_fail(doc, doc->input_length, "out of memory");
    doc->tokens = tokens;
    doc->token_capacity = requested;
    doc->owns_tokens = true;
    return true;
}

static inline bool brew_json_reserve_tokens(BrewJsonDocument *doc) {
    if (doc->token_count >= doc->limits.max_tokens)
        return brew_json_fail(doc, doc->input_length, "token limit exceeded");
    if (doc->token_count < doc->token_capacity) return true;
    size_t next = doc->token_capacity ? doc->token_capacity * 2 : 64;
    if (next < doc->token_capacity || next > doc->limits.max_tokens)
        next = doc->limits.max_tokens;
    if (next <= doc->token_capacity)
        return brew_json_fail(doc, doc->input_length, "token allocation overflow");
    return brew_json_document_reserve_tokens(doc, next);
}

static inline size_t brew_json_add_token(BrewJsonDocument *doc,
                                          BrewJsonKind kind, size_t start) {
    if (!brew_json_reserve_tokens(doc)) return BREW_JSON_NONE;
    size_t id = doc->token_count++;
    doc->tokens[id] = (BrewJsonToken){
        kind, start, start, BREW_JSON_NONE, BREW_JSON_NONE,
        BREW_JSON_NONE, 0
    };
    return id;
}

static inline void brew_json_add_child(BrewJsonDocument *doc,
                                        size_t parent, size_t child) {
    BrewJsonToken *p = &doc->tokens[parent];
    if (p->last_child == BREW_JSON_NONE) p->first_child = child;
    else doc->tokens[p->last_child].next_sibling = child;
    p->last_child = child;
    p->child_count++;
}

static inline bool brew_json_utf8(const unsigned char *s, size_t n,
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
    BrewJsonDocument *doc;
    size_t pos;
} BrewJsonParser;

static inline void brew_json_ws(BrewJsonParser *p) {
    const unsigned char *s = p->doc->input;
    while (p->pos < p->doc->input_length &&
           (s[p->pos] == ' ' || s[p->pos] == '\t' ||
            s[p->pos] == '\r' || s[p->pos] == '\n')) p->pos++;
}

static inline int brew_json_hex(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static inline bool brew_json_u16(BrewJsonParser *p, uint32_t *value) {
    if (p->doc->input_length - p->pos < 4) return false;
    uint32_t v = 0;
    for (unsigned i = 0; i < 4; i++) {
        int h = brew_json_hex(p->doc->input[p->pos++]);
        if (h < 0) return false;
        v = (v << 4) | (uint32_t)h;
    }
    *value = v;
    return true;
}

static inline bool brew_json_string_scan(BrewJsonParser *p) {
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
            if (!brew_json_u16(p, &u)) return false;
            if (u >= 0xD800 && u <= 0xDBFF) {
                if (n - p->pos < 6 || s[p->pos] != '\\' ||
                    s[p->pos + 1] != 'u') return false;
                p->pos += 2;
                uint32_t low;
                if (!brew_json_u16(p, &low) ||
                    low < 0xDC00 || low > 0xDFFF) return false;
            } else if (u >= 0xDC00 && u <= 0xDFFF) return false;
            continue;
        }
        if (c >= 0x80) {
            size_t width;
            p->pos--;
            if (!brew_json_utf8(s + p->pos, n - p->pos, &width))
                return false;
            p->pos += width;
        }
    }
    return false;
}

static inline bool brew_json_parse_value(BrewJsonParser *p, size_t depth,
                                          size_t *result);

static inline bool brew_json_parse_compound(BrewJsonParser *p, size_t depth,
                                             size_t *result, bool object) {
    BrewJsonDocument *d = p->doc;
    const unsigned char *s = d->input;
    size_t id = brew_json_add_token(d, object ? BREW_JSON_OBJECT : BREW_JSON_ARRAY,
                                    p->pos);
    if (id == BREW_JSON_NONE) return false;
    unsigned char close = object ? '}' : ']';
    p->pos++;
    brew_json_ws(p);
    if (p->pos < d->input_length && s[p->pos] == close) {
        p->pos++;
        d->tokens[id].end = p->pos;
        *result = id;
        return true;
    }
    for (;;) {
        if (object) {
            size_t key_start = p->pos;
            if (!brew_json_string_scan(p))
                return brew_json_fail(d, p->pos, "invalid object key string");
            size_t key = brew_json_add_token(d, BREW_JSON_STRING, key_start);
            if (key == BREW_JSON_NONE) return false;
            d->tokens[key].end = p->pos;
            brew_json_add_child(d, id, key);
            brew_json_ws(p);
            if (p->pos >= d->input_length || s[p->pos++] != ':')
                return brew_json_fail(d, p->pos, "expected ':' after object key");
            brew_json_ws(p);
        }
        size_t child;
        if (!brew_json_parse_value(p, depth + 1, &child)) return false;
        brew_json_add_child(d, id, child);
        brew_json_ws(p);
        if (p->pos >= d->input_length)
            return brew_json_fail(d, p->pos, "unterminated array or object");
        unsigned char c = s[p->pos++];
        if (c == close) break;
        if (c != ',')
            return brew_json_fail(d, p->pos - 1, "expected ',' or closing delimiter");
        brew_json_ws(p);
    }
    if (object && (d->tokens[id].child_count & 1u))
        return brew_json_fail(d, p->pos, "object key has no value");
    d->tokens[id].end = p->pos;
    *result = id;
    return true;
}

static inline bool brew_json_number_scan(BrewJsonParser *p) {
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

static inline bool brew_json_parse_value(BrewJsonParser *p, size_t depth,
                                          size_t *result) {
    BrewJsonDocument *d = p->doc;
    const unsigned char *s = d->input;
    brew_json_ws(p);
    if (depth > d->limits.max_depth)
        return brew_json_fail(d, p->pos, "maximum nesting depth exceeded");
    if (p->pos >= d->input_length)
        return brew_json_fail(d, p->pos, "expected JSON value");
    size_t start = p->pos, id;
    unsigned char c = s[p->pos];
    if (c == '{') return brew_json_parse_compound(p, depth, result, true);
    if (c == '[') return brew_json_parse_compound(p, depth, result, false);
    if (c == '"') {
        if (!brew_json_string_scan(p))
            return brew_json_fail(d, p->pos, "invalid JSON string or Unicode escape");
        id = brew_json_add_token(d, BREW_JSON_STRING, start);
        if (id == BREW_JSON_NONE) return false;
        d->tokens[id].end = p->pos;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        if (!brew_json_number_scan(p))
            return brew_json_fail(d, p->pos, "invalid JSON number");
        id = brew_json_add_token(d, BREW_JSON_NUMBER, start);
        if (id == BREW_JSON_NONE) return false;
        d->tokens[id].end = p->pos;
    } else {
        BrewJsonKind kind;
        const char *word;
        size_t length;
        if (c == 't') { kind = BREW_JSON_TRUE; word = "true"; length = 4; }
        else if (c == 'f') { kind = BREW_JSON_FALSE; word = "false"; length = 5; }
        else if (c == 'n') { kind = BREW_JSON_NULL; word = "null"; length = 4; }
        else return brew_json_fail(d, p->pos, "unexpected character");
        if (length > d->input_length - p->pos ||
            memcmp(s + p->pos, word, length) != 0)
            return brew_json_fail(d, p->pos, "invalid JSON literal");
        p->pos += length;
        id = brew_json_add_token(d, kind, start);
        if (id == BREW_JSON_NONE) return false;
        d->tokens[id].end = p->pos;
    }
    *result = id;
    return true;
}

/* Input is borrowed; it must remain alive until document_destroy or reparse. */
static inline bool brew_json_parse_reuse(BrewJsonDocument *doc,
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
        return brew_json_fail(doc, 0, "input size limit exceeded");
    BrewJsonParser parser = { doc, 0 };
    size_t root;
    if (!brew_json_parse_value(&parser, 0, &root)) return false;
    brew_json_ws(&parser);
    if (parser.pos != length)
        return brew_json_fail(doc, parser.pos, "trailing data after JSON value");
    return root == 0;
}

/* Reset and parse while retaining allocated token capacity. */
static inline bool brew_json_parse(BrewJsonDocument *doc,
                                    const void *input, size_t length) {
    return brew_json_parse_reuse(doc, input, length);
}
static inline bool brew_json_token_text(const BrewJsonDocument *doc,
                                         size_t token, const char **text,
                                         size_t *length) {
    if (!doc || token >= doc->token_count) return false;
    const BrewJsonToken *t = &doc->tokens[token];
    if (t->kind != BREW_JSON_STRING && t->kind != BREW_JSON_NUMBER)
        return false;
    size_t start = t->start, end = t->end;
    if (t->kind == BREW_JSON_STRING) { start++; end--; }
    if (text) *text = (const char *)doc->input + start;
    if (length) *length = end - start;
    return true;
}

static inline bool brew_json_key_equals(const BrewJsonDocument *doc,
                                         size_t token, const char *key) {
    const char *text;
    size_t length;
    if (!key || !brew_json_token_text(doc, token, &text, &length)) return false;
    size_t i = 0, j = 0, key_length = strlen(key);
    while (i < length) {
        size_t raw_start = i;
        uint32_t cp = (unsigned char)text[i++];
        if (cp >= 0x80) {
            size_t width;
            if (!brew_json_utf8((const unsigned char *)text + raw_start,
                                length - raw_start, &width)) return false;
            if (width == 2) {
                cp = ((uint32_t)(unsigned char)text[raw_start] & 0x1Fu) << 6;
                cp |= (uint32_t)(unsigned char)text[raw_start + 1] & 0x3Fu;
            } else if (width == 3) {
                cp = ((uint32_t)(unsigned char)text[raw_start] & 0x0Fu) << 12;
                cp |= ((uint32_t)(unsigned char)text[raw_start + 1] & 0x3Fu) << 6;
                cp |= (uint32_t)(unsigned char)text[raw_start + 2] & 0x3Fu;
            } else {
                cp = ((uint32_t)(unsigned char)text[raw_start] & 0x07u) << 18;
                cp |= ((uint32_t)(unsigned char)text[raw_start + 1] & 0x3Fu) << 12;
                cp |= ((uint32_t)(unsigned char)text[raw_start + 2] & 0x3Fu) << 6;
                cp |= (uint32_t)(unsigned char)text[raw_start + 3] & 0x3Fu;
            }
            i = raw_start + width;
        }
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
                        int h = brew_json_hex((unsigned char)text[i++]);
                        if (h < 0) return false;
                        cp = (cp << 4) | (uint32_t)h;
                    }
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (length - i < 6 || text[i++] != '\\' ||
                            text[i++] != 'u') return false;
                        uint32_t low = 0;
                        for (unsigned n = 0; n < 4; n++) {
                            int h = brew_json_hex((unsigned char)text[i++]);
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
static inline size_t brew_json_object_get(const BrewJsonDocument *doc,
                                           size_t object, const char *key) {
    if (!doc || object >= doc->token_count ||
        doc->tokens[object].kind != BREW_JSON_OBJECT) return BREW_JSON_NONE;
    size_t k = doc->tokens[object].first_child;
    while (k != BREW_JSON_NONE) {
        size_t value = doc->tokens[k].next_sibling;
        if (value == BREW_JSON_NONE) return BREW_JSON_NONE;
        if (brew_json_key_equals(doc, k, key)) return value;
        k = doc->tokens[value].next_sibling;
    }
    return BREW_JSON_NONE;
}

static inline bool brew_json_as_i64(const BrewJsonDocument *doc,
                                     size_t token, int64_t *value) {
    const char *text;
    size_t length;
    if (!value || !brew_json_token_text(doc, token, &text, &length) ||
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

/* Convert a JSON number to double without accepting partial parses or
 * overflowing/underflowing the host representation. JSON itself permits
 * exponent and fractional forms; callers needing exact decimal arithmetic
 * should retain the original token text instead. */
static inline bool brew_json_as_f64(const BrewJsonDocument *doc,
                                     size_t token, double *value) {
    const char *text;
    size_t length;
    if (!value || !brew_json_token_text(doc, token, &text, &length) ||
        !length || length >= 128) return false;
    char tmp[128];
    memcpy(tmp, text, length);
    tmp[length] = 0;
    errno = 0;
    char *end = NULL;
    double parsed = strtod(tmp, &end);
    if (errno == ERANGE || end != tmp + length || !isfinite(parsed))
        return false;
    *value = parsed;
    return true;
}

static inline bool brew_json_as_bool(const BrewJsonDocument *doc,
                                      size_t token, bool *value) {
    if (!doc || !value || token >= doc->token_count) return false;
    BrewJsonKind kind = doc->tokens[token].kind;
    if (kind != BREW_JSON_TRUE && kind != BREW_JSON_FALSE) return false;
    *value = kind == BREW_JSON_TRUE;
    return true;
}

static inline bool brew_json_buffer_reserve(BrewJsonBuffer *b, size_t extra) {
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

static inline bool brew_json_buffer_append(BrewJsonBuffer *b,
                                            const void *data, size_t length) {
    if (!brew_json_buffer_reserve(b, length)) return false;
    if (length) memcpy(b->data + b->length, data, length);
    b->length += length;
    b->data[b->length] = 0;
    return true;
}

static inline bool brew_json_encode_token(const BrewJsonDocument *doc,
                                           size_t id, BrewJsonBuffer *out,
                                           bool pretty, size_t depth) {
    if (id >= doc->token_count) return false;
    const BrewJsonToken *t = &doc->tokens[id];
    if (t->kind == BREW_JSON_STRING || t->kind == BREW_JSON_NUMBER ||
        t->kind == BREW_JSON_TRUE || t->kind == BREW_JSON_FALSE ||
        t->kind == BREW_JSON_NULL)
        return brew_json_buffer_append(out, doc->input + t->start, t->end - t->start);
    bool object = t->kind == BREW_JSON_OBJECT;
    char open = object ? '{' : '[', close = object ? '}' : ']';
    if (!brew_json_buffer_append(out, &open, 1)) return false;
    size_t child = t->first_child, index = 0;
    while (child != BREW_JSON_NONE) {
        if (index && !brew_json_buffer_append(out, ",", 1)) return false;
        if (pretty) {
            if (!brew_json_buffer_append(out, "\n", 1)) return false;
            for (size_t n = 0; n < depth + 1; n++)
                if (!brew_json_buffer_append(out, "  ", 2)) return false;
        }
        if (!brew_json_encode_token(doc, child, out, pretty, depth + 1)) return false;
        child = doc->tokens[child].next_sibling;
        if (object) {
            if (child == BREW_JSON_NONE ||
                !brew_json_buffer_append(out, pretty ? ": " : ":", pretty ? 2 : 1))
                return false;
            if (!brew_json_encode_token(doc, child, out, pretty, depth + 1)) return false;
            child = doc->tokens[child].next_sibling;
        }
        index++;
    }
    if (pretty && index) {
        if (!brew_json_buffer_append(out, "\n", 1)) return false;
        for (size_t n = 0; n < depth; n++)
            if (!brew_json_buffer_append(out, "  ", 2)) return false;
    }
    return brew_json_buffer_append(out, &close, 1);
}

static inline bool brew_json_encode(const BrewJsonDocument *doc,
                                     BrewJsonBuffer *out, bool pretty) {
    if (!doc || !out || !doc->token_count) return false;
    out->length = 0;
    if (out->data && out->capacity) out->data[0] = 0;
    return brew_json_encode_token(doc, 0, out, pretty, 0);
}

static inline void brew_json_buffer_destroy(BrewJsonBuffer *buffer) {
    if (!buffer) return;
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

#ifdef __cplusplus
}
#endif
#endif
