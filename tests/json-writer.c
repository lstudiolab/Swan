#include "runtime/json.h"
#include "runtime/json_writer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_value(void *writer) {
    assert(swan_json_writer_begin_object(writer));
    assert(swan_json_writer_key(writer, "id"));
    assert(swan_json_writer_i64(writer, 42));
    assert(swan_json_writer_key(writer, "name"));
    assert(swan_json_writer_string(writer, "Swan \"language\"\n"));
    assert(swan_json_writer_key(writer, "ok"));
    assert(swan_json_writer_bool(writer, 1));
    assert(swan_json_writer_key(writer, "items"));
    assert(swan_json_writer_begin_array(writer));
    assert(swan_json_writer_i64(writer, 1));
    assert(swan_json_writer_null(writer));
    assert(swan_json_writer_bool(writer, 0));
    assert(swan_json_writer_begin_object(writer));
    assert(swan_json_writer_key(writer, "check"));
    assert(swan_json_writer_string(writer, "\xE2\x9C\x93"));
    assert(swan_json_writer_end_object(writer));
    assert(swan_json_writer_end_array(writer));
    assert(swan_json_writer_end_object(writer));
}

int main(void) {
    void *writer = swan_json_writer_create(16);
    assert(writer != NULL);
    write_value(writer);
    const char *encoded = swan_json_writer_finish(writer);
    assert(encoded != NULL);
    assert(swan_json_writer_length(writer) == (int64_t)strlen(encoded));
    assert(strstr(encoded, "\\\"language\\\"") != NULL);
    assert(strstr(encoded, "\\n") != NULL);

    SwanJsonDocument doc;
    swan_json_document_init(&doc);
    assert(swan_json_parse(&doc, encoded, strlen(encoded)));
    size_t id = swan_json_object_get(&doc, 0, "id");
    int64_t number = 0;
    assert(id != SWAN_JSON_NONE && swan_json_as_i64(&doc, id, &number));
    assert(number == 42);
    assert(swan_json_object_get(&doc, 0, "items") != SWAN_JSON_NONE);
    swan_json_document_destroy(&doc);
    swan_json_writer_destroy(writer);

    writer = swan_json_writer_create(4);
    assert(writer != NULL);
    assert(swan_json_writer_begin_object(writer));
    assert(swan_json_writer_key(writer, "incomplete"));
    assert(swan_json_writer_finish(writer) == NULL);
    assert(swan_json_writer_error(writer)[0] != '\0');
    swan_json_writer_destroy(writer);

    writer = swan_json_writer_create(1);
    assert(writer != NULL);
    assert(swan_json_writer_begin_array(writer));
    assert(!swan_json_writer_begin_object(writer));
    assert(swan_json_writer_error(writer)[0] != '\0');
    swan_json_writer_destroy(writer);

    writer = swan_json_writer_create(8);
    assert(writer != NULL);
    assert(!swan_json_writer_string(writer, "\xC0\xAF"));
    assert(swan_json_writer_error(writer)[0] != '\0');
    swan_json_writer_destroy(writer);

    puts("JSON writer tests passed");
    return 0;
}
