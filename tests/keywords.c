#include "swan/lexer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const char *const keywords[] = {
        "let","var","fn","external","struct","enum","type","if","else",
        "for","in","while","loop","match","return","break","continue",
        "guard","from","as","import","export","true","false","const",
        "null","self","pass","lazy","init","deinit","free","throw",
        "rethrow","store","borrow","out"
    };
    static const char *const removed[] = {
        "func","extern","manual","log","none","and","or","not","echo"
    };
    char source[512] = {0};
    for (size_t i=0;i<sizeof(keywords)/sizeof(keywords[0]);i++) {
        if (i) strcat(source," ");
        strcat(source,keywords[i]);
    }
    TokenStream ts={0};SwanError error={0};
    assert(swan_lex(source,&ts,&error));
    assert(sizeof(keywords)/sizeof(keywords[0])==37);
    assert(ts.count==38);
    for (size_t i=0;i<37;i++) {
        if (ts.items[i].kind==T_IDENT) {
            fprintf(stderr,"keyword was not reserved: %s\n",keywords[i]);
            return 1;
        }
    }
    free(ts.items);free(ts.text_storage);memset(&ts,0,sizeof(ts));
    source[0]=0;
    for (size_t i=0;i<sizeof(removed)/sizeof(removed[0]);i++) {
        if (i) strcat(source," ");
        strcat(source,removed[i]);
    }
    assert(swan_lex(source,&ts,&error));
    assert(ts.count==sizeof(removed)/sizeof(removed[0])+1);
    for (size_t i=0;i<sizeof(removed)/sizeof(removed[0]);i++) {
        if (ts.items[i].kind!=T_IDENT) {
            fprintf(stderr,"removed word is still reserved: %s\n",removed[i]);
            return 2;
        }
    }
    free(ts.items);free(ts.text_storage);
    puts("PASS: exact 37-word Swan keyword set and removed legacy words");
    return 0;
}
