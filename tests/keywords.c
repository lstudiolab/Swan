#include "swan/lexer.h"
#include "swan/ast.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    static const char *const keywords[] = {
        "func","struct","let","var","null","if","else","const","type","match",
        "guard","block","while","loop","true","false","break","continue","return",
        "as","from","self","pass","upto","init","deinit","enum","throw","throws",
        "rethrow","catch"
    };
    static const char *const identifiers[] = {
        "import","export","fn","out","store","manual","for","nil","none","namespace",
        "variant","protocol","thread","async","external","lazy","case","set","print",
        "free","borrow","and","or","not","extern","defer","bool","inline","static",
        "private","public","mod","in","int"
    };
    char source[2048] = {0};
    for (size_t i=0;i<sizeof(keywords)/sizeof(keywords[0]);i++) {
        if (i) strcat(source," ");
        strcat(source,keywords[i]);
    }
    TokenStream ts={0};SwanError error={0};
    assert(swan_lex(source,&ts,&error));
    assert(sizeof(keywords)/sizeof(keywords[0])==31);
    assert(ts.count==32);
    for (size_t i=0;i<31;i++) {
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
            fprintf(stderr,"non-keyword was incorrectly reserved: %s (kind=%d)\n",identifiers[i],(int)ts.items[i].kind);
            return 2;
        }
    }
    free(ts.items);free(ts.text_storage);
    puts("PASS: exact 31-word Swan keyword set; all other tested spellings are identifiers");
    return 0;
}
