#include "runtime/json.h"
#include <assert.h>

static void valid(const char *source) {
    BrewJsonDocument doc;
    brew_json_document_init(&doc);
    assert(brew_json_parse(&doc, source, strlen(source)));
    BrewJsonBuffer out = {0};
    assert(brew_json_encode(&doc, &out, false));
    BrewJsonDocument reparsed;
    brew_json_document_init(&reparsed);
    assert(brew_json_parse(&reparsed, out.data, out.length));
    brew_json_document_destroy(&reparsed);
    brew_json_buffer_destroy(&out);
    brew_json_document_destroy(&doc);
}

static void invalid(const char *source) {
    BrewJsonDocument doc;
    brew_json_document_init(&doc);
    assert(!brew_json_parse(&doc, source, strlen(source)));
    assert(doc.error[0] != '\0');
    brew_json_document_destroy(&doc);
}

int main(void) {
    const char *source =
        "{\"service\":\"brew\",\"workers\":128,\"ok\":true,"
        "\"items\":[1,-2,3.5,null,{\"name\":\"A\\uD83D\\uDE00\"}],"
        "\"escaped\\u004bey\":\"value\"}";
    BrewJsonDocument doc;
    brew_json_document_init(&doc);
    assert(brew_json_parse(&doc, source, strlen(source)));
    size_t root = 0;
    size_t workers = brew_json_object_get(&doc, root, "workers");
    int64_t number = 0;
    assert(workers != BREW_JSON_NONE);
    assert(brew_json_as_i64(&doc, workers, &number) && number == 128);
    size_t fraction = brew_json_object_get(&doc, 0, "items");
    assert(fraction != BREW_JSON_NONE);
    size_t item = doc.tokens[fraction].first_child;
    item = doc.tokens[item].next_sibling;
    item = doc.tokens[item].next_sibling;
    double decimal = 0.0;
    assert(brew_json_as_f64(&doc, item, &decimal) && decimal == 3.5);
    size_t ok = brew_json_object_get(&doc, 0, "ok");
    bool boolean = false;
    assert(brew_json_as_bool(&doc, ok, &boolean) && boolean);
    assert(!brew_json_as_i64(&doc, item, &number));
    assert(!brew_json_as_bool(&doc, item, &boolean));
    assert(brew_json_object_get(&doc, root, "escapedKey") != BREW_JSON_NONE);

    BrewJsonBuffer compact = {0}, pretty = {0};
    assert(brew_json_encode(&doc, &compact, false));
    assert(brew_json_encode(&doc, &pretty, true));
    assert(compact.length > 0 && pretty.length > compact.length);
    assert(strstr(compact.data, "\"workers\":128") != NULL);
    assert(strstr(pretty.data, "\n") != NULL);

    BrewJsonDocument roundtrip;
    brew_json_document_init(&roundtrip);
    assert(brew_json_parse(&roundtrip, compact.data, compact.length));
    brew_json_document_destroy(&roundtrip);
    brew_json_buffer_destroy(&compact);
    brew_json_buffer_destroy(&pretty);
    brew_json_document_destroy(&doc);

    /* The reusable parser retains its token allocation across requests. */
    BrewJsonDocument reusable;
    brew_json_document_init(&reusable);
    assert(brew_json_document_reserve_tokens(&reusable, 128));
    assert(reusable.token_capacity == 128);
    assert(brew_json_parse(&reusable, "{\"a\":[1,2,3]}", 13));
    size_t capacity = reusable.token_capacity;
    assert(capacity >= reusable.token_count);
    assert(brew_json_parse_reuse(&reusable, "{\"b\":4}", 7));
    assert(reusable.token_capacity == capacity);
    assert(brew_json_object_get(&reusable, 0, "b") != BREW_JSON_NONE);
    assert(!brew_json_parse_reuse(&reusable, "{", 1));
    assert(brew_json_parse_reuse(&reusable, "false", 5));
    brew_json_document_destroy(&reusable);

    /* Caller-owned storage gives bounded, allocation-free token parsing. */
    BrewJsonDocument fixed;
    BrewJsonToken fixed_tokens[8];
    brew_json_document_init(&fixed);
    assert(brew_json_document_use_token_storage(
        &fixed, fixed_tokens, sizeof(fixed_tokens) / sizeof(fixed_tokens[0])));
    assert(brew_json_parse(&fixed, "[1,2,3]", 7));
    assert(fixed.tokens == fixed_tokens && fixed.token_capacity == 8);
    assert(!brew_json_parse_reuse(&fixed, "[1,2,3,4,5,6,7,8]", 17));
    assert(strstr(fixed.error, "caller token buffer exhausted") != NULL);
    brew_json_document_destroy(&fixed);

    BrewJsonDocument limited;
    brew_json_document_init(&limited);
    limited.limits.max_depth = 1;
    assert(!brew_json_parse(&limited, "[[0]]", 5));
    assert(strstr(limited.error, "depth") != NULL);
    brew_json_document_destroy(&limited);

    const char utf8_key[] = "{\"caf\xc3\xa9\":1}";
    BrewJsonDocument unicode_keys;
    brew_json_document_init(&unicode_keys);
    assert(brew_json_parse(&unicode_keys, utf8_key, sizeof(utf8_key) - 1));
    assert(brew_json_object_get(&unicode_keys, 0, "caf\xc3\xa9") != BREW_JSON_NONE);
    brew_json_document_destroy(&unicode_keys);

    const char escaped_utf8_key[] = "{\"caf\\u00e9\":1}";
    brew_json_document_init(&unicode_keys);
    assert(brew_json_parse(&unicode_keys, escaped_utf8_key, sizeof(escaped_utf8_key) - 1));
    assert(brew_json_object_get(&unicode_keys, 0, "caf\xc3\xa9") != BREW_JSON_NONE);
    brew_json_document_destroy(&unicode_keys);

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
    BrewJsonDocument numeric;
    brew_json_document_init(&numeric);
    assert(brew_json_parse(&numeric, "[1.25,1e3,1e9999]", 17));
    double converted = 0.0;
    size_t first = numeric.tokens[0].first_child;
    assert(brew_json_as_f64(&numeric, first, &converted) && converted == 1.25);
    size_t second = numeric.tokens[first].next_sibling;
    assert(brew_json_as_f64(&numeric, second, &converted) && converted == 1000.0);
    size_t third = numeric.tokens[second].next_sibling;
    assert(!brew_json_as_f64(&numeric, third, &converted));
    brew_json_document_destroy(&numeric);
    puts("JSON runtime tests passed");
    return 0;
}
