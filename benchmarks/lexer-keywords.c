#include "swan/lexer.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *const words[] = {
    "func","struct","let","var","null","if","else","const","type","match",
    "guard","block","while","loop","true","false","break","continue","return",
    "as","from","self","pass","upto","init","deinit","enum","throw","throws",
    "rethrow","catch","import","export","print","identifier","printValue","async"
};
static volatile unsigned sink;

static unsigned linear_keyword(const char *word) {
    for (unsigned i = 0; i < 33; ++i)
        if (strcmp(word, words[i]) == 0) return i + 1;
    return 0;
}
static double seconds(void) {
    struct timespec t;
    timespec_get(&t, TIME_UTC);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
int main(void) {
    const unsigned rounds = 2000000;
    unsigned sum = 0;
    double start = seconds();
    for (unsigned r=0; r<rounds; ++r)
        sum += (unsigned)swan_keyword(words[r % (sizeof(words)/sizeof(words[0]))],
                                     strlen(words[r % (sizeof(words)/sizeof(words[0]))]));
    double dispatch = seconds() - start;
    sink = sum;
    sum = 0;
    start = seconds();
    for (unsigned r=0; r<rounds; ++r)
        sum += linear_keyword(words[r % (sizeof(words)/sizeof(words[0]))]);
    double linear = seconds() - start;
    sink = sum;
    printf("keyword dispatch: %.3f s\nlinear reference: %.3f s\n", dispatch, linear);
    printf("dispatch/reference: %.3fx\n", linear > 0.0 ? linear / dispatch : 0.0);
    return 0;
}
