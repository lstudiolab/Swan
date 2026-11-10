#include "swan/lexer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const char *const keywords[] = {
        "import","namespace","export","struct","enum","type","bool","variant",
        "func","thread","let","var","const","external","if","else","while",
        "for","in","loop","block","guard","defer","match","async","as",
        "return","break","continue","true","false","null","self","nil",
        "pass","lazy","from","protocol","case","set"
    };
    static const char *const identifiers[] = {
        "extern","manual","log","none","and","or","not","echo","init",
        "deinit","free","throw","rethrow","store","borrow"
    };
    char source[1024] = {0};
    for (size_t i=0;i<sizeof(keywords)/sizeof(keywords[0]);i++) {
        if (i) strcat(source," ");
        strcat(source,keywords[i]);
    }
    TokenStream ts={0};SwanError error={0};
    assert(swan_lex(source,&ts,&error));
    assert(sizeof(keywords)/sizeof(keywords[0])==40);
    assert(ts.count==41);
    for (size_t i=0;i<40;i++) {
        if (ts.items[i].kind==T_IDENT) {
            fprintf(stderr,"keyword was not reserved: %s\n",keywords[i]);
            return 1;
        }
    }
    free(ts.items);free(ts.text_storage);memset(&ts,0,sizeof(ts));
    source[0]=0;
    for (size_t i=0;i<sizeof(identifiers)/sizeof(identifiers[0]);i++) {
        if (i) strcat(source," ");
        strcat(source,identifiers[i]);
    }
    assert(swan_lex(source,&ts,&error));
    assert(ts.count==sizeof(identifiers)/sizeof(identifiers[0])+1);
    for (size_t i=0;i<sizeof(identifiers)/sizeof(identifiers[0]);i++) {
        if (ts.items[i].kind!=T_IDENT) {
            fprintf(stderr,"removed word is still reserved: %s\n",identifiers[i]);
            return 2;
        }
    }
    free(ts.items);free(ts.text_storage);memset(&ts,0,sizeof(ts));
    assert(swan_lex("fn out",&ts,&error));
    assert(ts.items[0].kind==T_FN&&ts.items[1].kind==T_OUT);
    free(ts.items);free(ts.text_storage);
    puts("PASS: exact 40-word Swan keyword set and deprecated syntax aliases");
    return 0;
}
