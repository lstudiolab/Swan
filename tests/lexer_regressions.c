#include "swan/lexer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void release_tokens(TokenStream *ts) {
    free(ts->items);
    free(ts->text_storage);
    memset(ts, 0, sizeof(*ts));
}

int main(void) {
    TokenStream ts = {0};
    SwanError error = {0};
    const char *multiline = "\"\"\"alpha\nbeta\"\"\" x";
    assert(swan_lex(multiline, &ts, &error));
    assert(ts.count == 3);
    assert(ts.items[0].kind == T_STRING);
    assert(strcmp(ts.items[0].text, "alpha\nbeta") == 0);
    assert(ts.items[0].line == 1 && ts.items[0].col == 1);
    assert(ts.items[1].kind == T_IDENT);
    assert(strcmp(ts.items[1].text, "x") == 0);
    assert(ts.items[1].line == 2 && ts.items[1].col == 9);
    release_tokens(&ts);

    const char *following = "\"first\\nsecond\"\nname";
    assert(swan_lex(following, &ts, &error));
    assert(ts.items[0].kind == T_STRING);
    assert(ts.items[1].kind == T_NEWLINE);
    assert(ts.items[2].kind == T_IDENT);
    assert(ts.items[2].line == 2 && ts.items[2].col == 1);
    release_tokens(&ts);

    const char malformed[] = {'x', '\n', (char)0xE2, '(', (char)0xA1, 0};
    assert(!swan_lex(malformed, &ts, &error));
    assert(error.line == 2 && error.col == 2);
    assert(strstr(error.message, "UTF-8") != NULL);

    const char unterminated[] = "\"\"\"open\nstring";
    assert(!swan_lex(unterminated, &ts, &error));
    assert(error.line == 1 && error.col == 1);
    assert(strstr(error.message, "triple-quoted") != NULL);

    puts("PASS: triple strings, multiline source locations, and malformed UTF-8 locations");
    return 0;
}
