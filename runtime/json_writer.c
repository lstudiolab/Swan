#include "json_writer.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bool object;
    bool first;
    bool expecting_value;
} SwanJsonWriterFrame;

typedef struct {
    SwanJsonBuffer output;
    SwanJsonWriterFrame frames[128];
    size_t depth;
    size_t max_depth;
    bool root_written;
    bool failed;
    char error[96];
} SwanJsonWriter;

static bool swan_json_writer_fail(SwanJsonWriter *writer, const char *message) {
    if (writer) {
        writer->failed = true;
        snprintf(writer->error, sizeof(writer->error), "%s", message);
    }
    return false;
}

static bool swan_json_writer_raw(SwanJsonWriter *writer,
                                 const char *data, size_t length) {
    if (!writer || writer->failed ||
        !swan_json_buffer_append(&writer->output, data, length))
        return swan_json_writer_fail(writer, "JSON writer allocation failed");
    return true;
}

static bool swan_json_writer_before_value(SwanJsonWriter *writer) {
    if (!writer || writer->failed) return false;
    if (!writer->depth) {
        if (writer->root_written)
            return swan_json_writer_fail(writer, "multiple root JSON values");
        writer->root_written = true;
        return true;
    }
    SwanJsonWriterFrame *frame = &writer->frames[writer->depth - 1];
    if (frame->object) {
        if (!frame->expecting_value)
            return swan_json_writer_fail(writer, "object expects a key");
        frame->expecting_value = false;
        return true;
    }
    if (!frame->first && !swan_json_writer_raw(writer, ",", 1)) return false;
    frame->first = false;
    return true;
}

static bool swan_json_writer_quoted(SwanJsonWriter *writer, const char *value) {
    if (!value) return swan_json_writer_fail(writer, "null string pointer");
    if (!swan_json_writer_raw(writer, "\"", 1)) return false;
    const unsigned char *p = (const unsigned char *)value;
    size_t length = strlen(value), i = 0;
    static const char hex[] = "0123456789abcdef";
    while (i < length) {
        unsigned char c = p[i];
        if (c == '"' || c == '\\') {
            char escaped[2] = {'\\', (char)c};
            if (!swan_json_writer_raw(writer, escaped, 2)) return false;
            i++;
        } else if (c == '\b' || c == '\f' || c == '\n' ||
                   c == '\r' || c == '\t') {
            char escaped[2] = {'\\', c == '\b' ? 'b' : c == '\f' ? 'f' :
                                    c == '\n' ? 'n' : c == '\r' ? 'r' : 't'};
            if (!swan_json_writer_raw(writer, escaped, 2)) return false;
            i++;
        } else if (c < 0x20) {
            char escaped[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
            if (!swan_json_writer_raw(writer, escaped, 6)) return false;
            i++;
        } else if (c < 0x80) {
            if (!swan_json_writer_raw(writer, (const char *)p + i, 1)) return false;
            i++;
        } else {
            size_t width;
            if (!swan_json_utf8(p + i, length - i, &width))
                return swan_json_writer_fail(writer, "invalid UTF-8 string");
            if (!swan_json_writer_raw(writer, (const char *)p + i, width))
                return false;
            i += width;
        }
    }
    return swan_json_writer_raw(writer, "\"", 1);
}

void *swan_json_writer_create(int64_t max_depth) {
    if (max_depth < 0 || max_depth > 128) return NULL;
    SwanJsonWriter *writer = (SwanJsonWriter *)calloc(1, sizeof(*writer));
    if (!writer) return NULL;
    writer->max_depth = max_depth ? (size_t)max_depth : 128;
    return writer;
}

int32_t swan_json_writer_begin_object(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    if (!swan_json_writer_before_value(writer) ||
        writer->depth >= writer->max_depth ||
        !swan_json_writer_raw(writer, "{", 1)) {
        if (writer && writer->depth >= writer->max_depth)
            swan_json_writer_fail(writer, "maximum writer depth exceeded");
        return 0;
    }
    writer->frames[writer->depth++] = (SwanJsonWriterFrame){true, true, false};
    return 1;
}

int32_t swan_json_writer_end_object(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    if (!writer || writer->failed || !writer->depth)
        return swan_json_writer_fail(writer, "unexpected object close");
    SwanJsonWriterFrame *frame = &writer->frames[writer->depth - 1];
    if (!frame->object || frame->expecting_value)
        return swan_json_writer_fail(writer, "object has an incomplete key or wrong close");
    writer->depth--;
    return swan_json_writer_raw(writer, "}", 1);
}

int32_t swan_json_writer_begin_array(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    if (!swan_json_writer_before_value(writer) ||
        writer->depth >= writer->max_depth ||
        !swan_json_writer_raw(writer, "[", 1)) {
        if (writer && writer->depth >= writer->max_depth)
            swan_json_writer_fail(writer, "maximum writer depth exceeded");
        return 0;
    }
    writer->frames[writer->depth++] = (SwanJsonWriterFrame){false, true, false};
    return 1;
}

int32_t swan_json_writer_end_array(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    if (!writer || writer->failed || !writer->depth)
        return swan_json_writer_fail(writer, "unexpected array close");
    if (writer->frames[writer->depth - 1].object)
        return swan_json_writer_fail(writer, "array closed while object is open");
    writer->depth--;
    return swan_json_writer_raw(writer, "]", 1);
}

int32_t swan_json_writer_key(void *value, const char *key) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    if (!writer || writer->failed || !writer->depth)
        return swan_json_writer_fail(writer, "key outside object");
    SwanJsonWriterFrame *frame = &writer->frames[writer->depth - 1];
    if (!frame->object || frame->expecting_value)
        return swan_json_writer_fail(writer, "object is not ready for a key");
    if (!frame->first && !swan_json_writer_raw(writer, ",", 1)) return 0;
    if (!swan_json_writer_quoted(writer, key) ||
        !swan_json_writer_raw(writer, ":", 1)) return 0;
    frame->first = false;
    frame->expecting_value = true;
    return 1;
}

int32_t swan_json_writer_string(void *value, const char *text) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    return swan_json_writer_before_value(writer) &&
           swan_json_writer_quoted(writer, text);
}

int32_t swan_json_writer_i64(void *value, int64_t number) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    if (!swan_json_writer_before_value(writer)) return 0;
    char buffer[32];
    int length = snprintf(buffer, sizeof(buffer), "%lld", (long long)number);
    return length > 0 && (size_t)length < sizeof(buffer) &&
           swan_json_writer_raw(writer, buffer, (size_t)length);
}

int32_t swan_json_writer_bool(void *value, int32_t boolean) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    const char *text = boolean ? "true" : "false";
    return swan_json_writer_before_value(writer) &&
           swan_json_writer_raw(writer, text, boolean ? 4 : 5);
}

int32_t swan_json_writer_null(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    return swan_json_writer_before_value(writer) &&
           swan_json_writer_raw(writer, "null", 4);
}

const char *swan_json_writer_finish(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    if (!writer || writer->failed || writer->depth || !writer->root_written) {
        if (writer && !writer->failed)
            swan_json_writer_fail(writer, "incomplete JSON document");
        return NULL;
    }
    return writer->output.data;
}

int64_t swan_json_writer_length(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    return writer && !writer->failed && writer->depth == 0 &&
           writer->root_written ? (int64_t)writer->output.length : -1;
}

const char *swan_json_writer_error(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    return writer ? writer->error : "invalid JSON writer";
}

void swan_json_writer_destroy(void *value) {
    SwanJsonWriter *writer = (SwanJsonWriter *)value;
    if (!writer) return;
    swan_json_buffer_destroy(&writer->output);
    free(writer);
}
