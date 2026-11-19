#include "json_writer.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool object;
    bool first;
    bool expecting_value;
} BrewJsonWriterFrame;

typedef struct {
    BrewJsonBuffer output;
    BrewJsonWriterFrame frames[128];
    size_t depth;
    size_t max_depth;
    bool root_written;
    bool failed;
    char error[96];
} BrewJsonWriter;

static bool brew_json_writer_fail(BrewJsonWriter *writer, const char *message) {
    if (writer) {
        writer->failed = true;
        snprintf(writer->error, sizeof(writer->error), "%s", message);
    }
    return false;
}

static bool brew_json_writer_raw(BrewJsonWriter *writer,
                                 const char *data, size_t length) {
    if (!writer || writer->failed) return false;
    if (!brew_json_buffer_append(&writer->output, data, length))
        return brew_json_writer_fail(writer, "JSON writer allocation failed");
    return true;
}

static bool brew_json_writer_before_value(BrewJsonWriter *writer) {
    if (!writer || writer->failed) return false;
    if (!writer->depth) {
        if (writer->root_written)
            return brew_json_writer_fail(writer, "multiple root JSON values");
        writer->root_written = true;
        return true;
    }
    BrewJsonWriterFrame *frame = &writer->frames[writer->depth - 1];
    if (frame->object) {
        if (!frame->expecting_value)
            return brew_json_writer_fail(writer, "object expects a key");
        frame->expecting_value = false;
        return true;
    }
    if (!frame->first && !brew_json_writer_raw(writer, ",", 1)) return false;
    frame->first = false;
    return true;
}

static bool brew_json_writer_quoted(BrewJsonWriter *writer, const char *value) {
    if (!value) return brew_json_writer_fail(writer, "null string pointer");
    if (!brew_json_writer_raw(writer, "\"", 1)) return false;
    const unsigned char *p = (const unsigned char *)value;
    size_t length = strlen(value), i = 0;
    static const char hex[] = "0123456789abcdef";
    while (i < length) {
        unsigned char c = p[i];
        if (c == '"' || c == '\\') {
            char escaped[2] = {'\\', (char)c};
            if (!brew_json_writer_raw(writer, escaped, 2)) return false;
            i++;
        } else if (c == '\b' || c == '\f' || c == '\n' ||
                   c == '\r' || c == '\t') {
            char escaped[2] = {'\\', c == '\b' ? 'b' : c == '\f' ? 'f' :
                                    c == '\n' ? 'n' : c == '\r' ? 'r' : 't'};
            if (!brew_json_writer_raw(writer, escaped, 2)) return false;
            i++;
        } else if (c < 0x20) {
            char escaped[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
            if (!brew_json_writer_raw(writer, escaped, 6)) return false;
            i++;
        } else if (c < 0x80) {
            if (!brew_json_writer_raw(writer, (const char *)p + i, 1)) return false;
            i++;
        } else {
            size_t width;
            if (!brew_json_utf8(p + i, length - i, &width))
                return brew_json_writer_fail(writer, "invalid UTF-8 string");
            if (!brew_json_writer_raw(writer, (const char *)p + i, width))
                return false;
            i += width;
        }
    }
    return brew_json_writer_raw(writer, "\"", 1);
}

void *brew_json_writer_create(int64_t max_depth) {
    if (max_depth < 0 || max_depth > 128) return NULL;
    BrewJsonWriter *writer = (BrewJsonWriter *)calloc(1, sizeof(*writer));
    if (!writer) return NULL;
    writer->max_depth = max_depth ? (size_t)max_depth : 128;
    return writer;
}

int32_t brew_json_writer_begin_object(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    if (!brew_json_writer_before_value(writer) ||
        writer->depth >= writer->max_depth ||
        !brew_json_writer_raw(writer, "{", 1)) {
        if (writer && writer->depth >= writer->max_depth)
            brew_json_writer_fail(writer, "maximum writer depth exceeded");
        return 0;
    }
    writer->frames[writer->depth++] = (BrewJsonWriterFrame){true, true, false};
    return 1;
}

int32_t brew_json_writer_end_object(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    if (!writer || writer->failed || !writer->depth)
        return brew_json_writer_fail(writer, "unexpected object close");
    BrewJsonWriterFrame *frame = &writer->frames[writer->depth - 1];
    if (!frame->object || frame->expecting_value)
        return brew_json_writer_fail(writer, "object has an incomplete key or wrong close");
    writer->depth--;
    return brew_json_writer_raw(writer, "}", 1);
}

int32_t brew_json_writer_begin_array(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    if (!brew_json_writer_before_value(writer) ||
        writer->depth >= writer->max_depth ||
        !brew_json_writer_raw(writer, "[", 1)) {
        if (writer && writer->depth >= writer->max_depth)
            brew_json_writer_fail(writer, "maximum writer depth exceeded");
        return 0;
    }
    writer->frames[writer->depth++] = (BrewJsonWriterFrame){false, true, false};
    return 1;
}

int32_t brew_json_writer_end_array(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    if (!writer || writer->failed || !writer->depth)
        return brew_json_writer_fail(writer, "unexpected array close");
    if (writer->frames[writer->depth - 1].object)
        return brew_json_writer_fail(writer, "array closed while object is open");
    writer->depth--;
    return brew_json_writer_raw(writer, "]", 1);
}

int32_t brew_json_writer_key(void *value, const char *key) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    if (!writer || writer->failed || !writer->depth)
        return brew_json_writer_fail(writer, "key outside object");
    BrewJsonWriterFrame *frame = &writer->frames[writer->depth - 1];
    if (!frame->object || frame->expecting_value)
        return brew_json_writer_fail(writer, "object is not ready for a key");
    if (!frame->first && !brew_json_writer_raw(writer, ",", 1)) return 0;
    if (!brew_json_writer_quoted(writer, key) ||
        !brew_json_writer_raw(writer, ":", 1)) return 0;
    frame->first = false;
    frame->expecting_value = true;
    return 1;
}

int32_t brew_json_writer_string(void *value, const char *text) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    return brew_json_writer_before_value(writer) &&
           brew_json_writer_quoted(writer, text);
}

int32_t brew_json_writer_i64(void *value, int64_t number) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    if (!brew_json_writer_before_value(writer)) return 0;
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%lld", (long long)number);
    return length > 0 && (size_t)length < sizeof(buffer) &&
           brew_json_writer_raw(writer, buffer, (size_t)length);
}

int32_t brew_json_writer_bool(void *value, int32_t boolean) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    const char *text = boolean ? "true" : "false";
    return brew_json_writer_before_value(writer) &&
           brew_json_writer_raw(writer, text, boolean ? 4 : 5);
}

int32_t brew_json_writer_null(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    return brew_json_writer_before_value(writer) &&
           brew_json_writer_raw(writer, "null", 4);
}

const char *brew_json_writer_finish(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    if (!writer || writer->failed || writer->depth || !writer->root_written) {
        if (writer && !writer->failed)
            brew_json_writer_fail(writer, "incomplete JSON document");
        return NULL;
    }
    return writer->output.data;
}

int64_t brew_json_writer_length(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    return writer && !writer->failed && writer->depth == 0 &&
           writer->root_written ? (int64_t)writer->output.length : -1;
}

const char *brew_json_writer_error(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    return writer ? writer->error : "invalid JSON writer";
}

void brew_json_writer_destroy(void *value) {
    BrewJsonWriter *writer = (BrewJsonWriter *)value;
    if (!writer) return;
    brew_json_buffer_destroy(&writer->output);
    free(writer);
}
