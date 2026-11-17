#include "runtime/json.h"
#include <assert.h>

static void valid(const char *source) {
    SwanJsonDocument doc;
    swan_json_document_init(&doc);
    assert(swan_json_parse(&doc, source, strlen(source)));
    SwanJsonBuffer out = {0};
    assert(swan_json_encode(&doc, &out, false));
    SwanJsonDocument reparsed;
    swan_json_document_init(&reparsed);
    assert(swan_json_parse(&reparsed, out.data, out.length));
    swan_json_document_destroy(&reparsed);
    swan_json_buffer_destroy(&out);
    swan_json_document_destroy(&doc);
}

static void invalid(const char *source) {
    SwanJsonDocument doc;
    swan_json_document_init(&doc);
    assert(!swan_json_parse(&doc, source, strlen(source)));
    assert(doc.error[0] != '\0');
    swan_json_document_destroy(&doc);
}

int main(void) {
    const char *source =
        "{\"service\":\"swan\",\"workers\":128,\"ok\":true,"
        "\"items\":[1,-2,3.5,null,{\"name\":\"A\\uD83D\\uDE00\"}],"
        "\"escaped\\u004bey\":\"value\"}";
    SwanJsonDocument doc;
    swan_json_document_init(&doc);
    assert(swan_json_parse(&doc, source, strlen(source)));
    size_t root = 0;
    size_t workers = swan_json_object_get(&doc, root, "workers");
    int64_t number = 0;
    assert(workers != SWAN_JSON_NONE);
    assert(swan_json_as_i64(&doc, workers, &number) && number == 128);
    assert(swan_json_object_get(&doc, root, "escapedKey") != SWAN_JSON_NONE);

    SwanJsonBuffer compact = {0}, pretty = {0};
    assert(swan_json_encode(&doc, &compact, false));
    assert(swan_json_encode(&doc, &pretty, true));
    assert(compact.length > 0 && pretty.length > compact.length);
    assert(strstr(compact.data, "\"workers\":128") != NULL);
    assert(strstr(pretty.data, "\n") != NULL);

    SwanJsonDocument roundtrip;
    swan_json_document_init(&roundtrip);
    assert(swan_json_parse(&roundtrip, compact.data, compact.length));
    swan_json_document_destroy(&roundtrip);
    swan_json_buffer_destroy(&compact);
    swan_json_buffer_destroy(&pretty);
    swan_json_document_destroy(&doc);

    valid("null");
    valid("true");
    valid("[]");
    valid("{}");
    valid("[0,-0,1.0,1e+3]");
    valid("\"valid UTF-8: \xE2\x9C\x93\"");
    invalid("");
    invalid("{");
    invalid("[1,]");
    invalid("{\"a\":}");
    invalid("{\"a\":1,}");
    invalid("01");
    invalid("1.");
    invalid("1e");
    invalid("\"\\uD800\"");
    invalid("\"\\uDC00\"");
    invalid("\"\\uD800\\u0041\"");
    invalid("\"raw\nnewline\"");
    invalid("\"bad UTF-8: \xC0\xAF\"");
    invalid("true false");
    puts("JSON runtime tests passed");
    return 0;
}
