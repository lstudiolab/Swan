#include "BrewRuntime/json.h"
#include "BrewRuntime/json_writer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_value(void *writer) {
    assert(brew_json_writer_begin_object(writer));
    assert(brew_json_writer_key(writer, "id"));
    assert(brew_json_writer_i64(writer, 42));
    assert(brew_json_writer_key(writer, "name"));
    assert(brew_json_writer_string(writer, "Brew \"language\"\n"));
    assert(brew_json_writer_key(writer, "ok"));
    assert(brew_json_writer_bool(writer, 1));
    assert(brew_json_writer_key(writer, "items"));
    assert(brew_json_writer_begin_array(writer));
    assert(brew_json_writer_i64(writer, 1));
    assert(brew_json_writer_null(writer));
    assert(brew_json_writer_bool(writer, 0));
    assert(brew_json_writer_begin_object(writer));
    assert(brew_json_writer_key(writer, "check"));
    assert(brew_json_writer_string(writer, "\xE2\x9C\x93"));
    assert(brew_json_writer_end_object(writer));
    assert(brew_json_writer_end_array(writer));
    assert(brew_json_writer_end_object(writer));
}

int main(void) {
    void *writer = brew_json_writer_create(16);
    assert(writer != NULL);
    write_value(writer);
    const char *encoded = brew_json_writer_finish(writer);
    assert(encoded != NULL);
    assert(brew_json_writer_length(writer) == (int64_t)strlen(encoded));
    assert(strstr(encoded, "\\\"language\\\"") != NULL);
    assert(strstr(encoded, "\\n") != NULL);

    BrewJsonDocument doc;
    brew_json_document_init(&doc);
    assert(brew_json_parse(&doc, encoded, strlen(encoded)));
    size_t id = brew_json_object_get(&doc, 0, "id");
    int64_t number = 0;
    assert(id != BREW_JSON_NONE && brew_json_as_i64(&doc, id, &number));
    assert(number == 42);
    assert(brew_json_object_get(&doc, 0, "items") != BREW_JSON_NONE);
    brew_json_document_destroy(&doc);
    brew_json_writer_destroy(writer);

    writer = brew_json_writer_create(4);
    assert(writer != NULL);
    assert(brew_json_writer_begin_object(writer));
    assert(brew_json_writer_key(writer, "incomplete"));
    assert(brew_json_writer_finish(writer) == NULL);
    assert(brew_json_writer_error(writer)[0] != '\0');
    brew_json_writer_destroy(writer);

    writer = brew_json_writer_create(1);
    assert(writer != NULL);
    assert(brew_json_writer_begin_array(writer));
    assert(!brew_json_writer_begin_object(writer));
    assert(brew_json_writer_error(writer)[0] != '\0');
    brew_json_writer_destroy(writer);

    writer = brew_json_writer_create(8);
    assert(writer != NULL);
    assert(!brew_json_writer_string(writer, "\xC0\xAF"));
    assert(brew_json_writer_error(writer)[0] != '\0');
    brew_json_writer_destroy(writer);

    puts("JSON writer tests passed");
    return 0;
}
