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
    size_t fraction = swan_json_object_get(&doc, 0, "items");
    assert(fraction != SWAN_JSON_NONE);
    size_t item = doc.tokens[fraction].first_child;
    item = doc.tokens[item].next_sibling;
    item = doc.tokens[item].next_sibling;
    double decimal = 0.0;
    assert(swan_json_as_f64(&doc, item, &decimal) && decimal == 3.5);
    size_t ok = swan_json_object_get(&doc, 0, "ok");
    bool boolean = false;
    assert(swan_json_as_bool(&doc, ok, &boolean) && boolean);
    assert(!swan_json_as_i64(&doc, item, &number));
    assert(!swan_json_as_bool(&doc, item, &boolean));
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

    /* The reusable parser retains its token allocation across requests. */
    SwanJsonDocument reusable;
    swan_json_document_init(&reusable);
    assert(swan_json_document_reserve_tokens(&reusable, 128));
    assert(reusable.token_capacity == 128);
    assert(swan_json_parse(&reusable, "{\"a\":[1,2,3]}", 13));
    size_t capacity = reusable.token_capacity;
    assert(capacity >= reusable.token_count);
    assert(swan_json_parse_reuse(&reusable, "{\"b\":4}", 7));
    assert(reusable.token_capacity == capacity);
    assert(swan_json_object_get(&reusable, 0, "b") != SWAN_JSON_NONE);
    assert(!swan_json_parse_reuse(&reusable, "{", 1));
    assert(swan_json_parse_reuse(&reusable, "false", 5));
    swan_json_document_destroy(&reusable);

    SwanJsonDocument limited;
    swan_json_document_init(&limited);
    limited.limits.max_depth = 1;
    assert(!swan_json_parse(&limited, "[[0]]", 5));
    assert(strstr(limited.error, "depth") != NULL);
    swan_json_document_destroy(&limited);

    const char utf8_key[] = "{\"caf\xc3\xa9\":1}";
    SwanJsonDocument unicode_keys;
    swan_json_document_init(&unicode_keys);
    assert(swan_json_parse(&unicode_keys, utf8_key, sizeof(utf8_key) - 1));
    assert(swan_json_object_get(&unicode_keys, 0, "caf\xc3\xa9") != SWAN_JSON_NONE);
    swan_json_document_destroy(&unicode_keys);

    const char escaped_utf8_key[] = "{\"caf\\u00e9\":1}";
    swan_json_document_init(&unicode_keys);
    assert(swan_json_parse(&unicode_keys, escaped_utf8_key, sizeof(escaped_utf8_key) - 1));
    assert(swan_json_object_get(&unicode_keys, 0, "caf\xc3\xa9") != SWAN_JSON_NONE);
    swan_json_document_destroy(&unicode_keys);

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

    /* Numeric conversion rejects overflow and partial numeric forms. */
    SwanJsonDocument numeric;
    swan_json_document_init(&numeric);
    assert(swan_json_parse(&numeric, "[1.25,1e3,1e9999]", 17));
    double converted = 0.0;
    size_t first = numeric.tokens[0].first_child;
    assert(swan_json_as_f64(&numeric, first, &converted) && converted == 1.25);
    size_t second = numeric.tokens[first].next_sibling;
    assert(swan_json_as_f64(&numeric, second, &converted) && converted == 1000.0);
    size_t third = numeric.tokens[second].next_sibling;
    assert(!swan_json_as_f64(&numeric, third, &converted));
    swan_json_document_destroy(&numeric);
    puts("JSON runtime tests passed");
    return 0;
}
