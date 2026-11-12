#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern int64_t swan_fn_swanc_parser_test(
    uint32_t *source, int64_t source_length,
    uint32_t *tokens, uint32_t *nodes, uint32_t *state);
extern int64_t swan_fn_swanc_lex_test(
    uint32_t *source, int64_t source_length,
    uint32_t *tokens, uint32_t *diagnostic);

static int parse(const char *text, uint32_t *nodes, uint32_t *state) {
    uint32_t source[128] = {0};
    uint32_t tokens[128 * 5] = {0};
    size_t length = strlen(text);
    if (length > 128) return -99;
    for (size_t i = 0; i < length; ++i) source[i] = (unsigned char)text[i];
    memset(nodes, 0, 128 * 5 * sizeof(uint32_t));
    state[0] = state[1] = 0;
    return (int)swan_fn_swanc_parser_test(source, (int64_t)length,
        tokens, nodes, state);
}

static int arithmetic_precedence(void) {
    uint32_t nodes[128 * 5] = {0}, state[4] = {0};
    int root = parse("2 + 3 * (4 - 1)", nodes, state);
    if (root != 7 || state[1] != 7) {
        fprintf(stderr, "precedence parse failed: root=%d nodes=%u\n", root, state[1]);
        return 1;
    }
    /* Node 5 is 4-1, node 6 is 3*(4-1), node 7 is 2+... */
    const uint32_t *sub = &nodes[4 * 5];
    const uint32_t *mul = &nodes[5 * 5];
    const uint32_t *add = &nodes[6 * 5];
    if (sub[0] != 4 || sub[1] != 6 || sub[2] != 3 || sub[3] != 4 ||
        mul[0] != 4 || mul[1] != 3 || mul[2] != 2 || mul[3] != 5 ||
        add[0] != 4 || add[1] != 1 || add[2] != 1 || add[3] != 6) {
        fprintf(stderr, "incorrect AST shape for precedence expression\n");
        return 2;
    }
    return 0;
}

static int left_associativity(void) {
    uint32_t nodes[128 * 5] = {0}, state[4] = {0};
    int root = parse("10 - 3 - 2", nodes, state);
    if (root != 5 || nodes[3 * 5 + 1] != 1 ||
        nodes[3 * 5 + 2] != 1 || nodes[3 * 5 + 3] != 2 ||
        nodes[4 * 5 + 1] != 3 || nodes[4 * 5 + 2] != 4 ||
        nodes[4 * 5 + 3] != 3) {
        fprintf(stderr, "subtraction was not parsed left-associatively\n");
        return 3;
    }
    return 0;
}

static int unicode_columns(void) {
    /* UTF-8 é occupies two bytes but one source column. */
    uint32_t source[] = {0xC3, 0xA9, 32, 64};
    uint32_t tokens[20] = {0}, diagnostic[4] = {0};
    int64_t status = swan_fn_swanc_lex_test(source, 4, tokens, diagnostic);
    if (status != -2 || diagnostic[0] != 2 || diagnostic[1] != 3 ||
        diagnostic[2] != 1 || diagnostic[3] != 3) {
        fprintf(stderr, "UTF-8 diagnostic column incorrect: status=%lld offset=%u line=%u column=%u\\n",
            (long long)status, diagnostic[1], diagnostic[2], diagnostic[3]);
        return 8;
    }
    return 0;
}

static int parentheses_and_rejections(void) {
    uint32_t nodes[128 * 5] = {0}, state[4] = {0};
    if (parse("(12)", nodes, state) != 1 || nodes[0] != 1 || nodes[4] != 12)
        return 4;
    if (parse("1.5", nodes, state) >= 0) {
        fprintf(stderr, "floating literal unexpectedly accepted by integer parser\n");
        return 5;
    }
    if (parse("1 2", nodes, state) >= 0) {
        fprintf(stderr, "trailing expression unexpectedly accepted\n");
        return 6;
    }
    if (parse("1 + )", nodes, state) >= 0) {
        fprintf(stderr, "missing operand unexpectedly accepted\n");
        return 7;
    }
    return 0;
}

int main(void) {
    int status = arithmetic_precedence();
    if (status) return status;
    status = left_associativity();
    if (status) return status;
    status = parentheses_and_rejections();
    if (status) return status;
    status = unicode_columns();
    if (status) return status;
    puts("Compiler 2 parser regressions passed: lexer token ABI, precedence, associativity, parentheses, EOF, invalid expressions, and UTF-8 diagnostic columns.");
    return 0;
}
