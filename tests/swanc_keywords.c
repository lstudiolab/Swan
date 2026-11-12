#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern int64_t swan_fn_swanc_keyword(uint32_t *, int64_t, int64_t);
typedef struct { const char *word; uint32_t token; } Keyword;
static const Keyword keywords[] = {
 {"func",8},{"struct",52},{"import",9},{"export",62},{"enum",53},
 {"let",11},{"var",12},{"const",13},{"if",15},{"else",16},
 {"while",17},{"loop",57},{"true",21},{"false",22},{"guard",59},
 {"defer",81},{"return",14},{"break",18},{"continue",19},{"block",77},
 {"as",61},{"bool",79},{"from",60},{"upto",55},{"inline",64},
 {"static",70},{"private",72},{"public",73},{"match",58},{"mod",74},
 {"in",56},{"int",54},{"init",75},{"deinit",76},{"self",63},{"pass",20}
};
static const char *const identifiers[] = {
 "fn","out","store","manual","for","type","null","nil","none","namespace",
 "variant","protocol","thread","async","external","lazy","case","set",
 "print","free","throw","rethrow","borrow","and","or","not","extern",
 "functional","letx","Func"
};
static int classify(const char *word,uint32_t expected) {
 uint32_t bytes[32]={0};
 size_t n=strlen(word);
 for(size_t i=0;i<n;i++)bytes[i]=(unsigned char)word[i];
 int64_t got=swan_fn_swanc_keyword(bytes,0,(int64_t)n);
 if(got!=(int64_t)expected) {
  fprintf(stderr,"%s classified as %lld, expected %u\n",word,(long long)got,expected);
  return 1;
 }
 return 0;
}
int main(void) {
 if(sizeof(keywords)/sizeof(keywords[0])!=36)return 3;
 for(size_t i=0;i<sizeof(keywords)/sizeof(keywords[0]);i++)
  if(classify(keywords[i].word,keywords[i].token))return 1;
 for(size_t i=0;i<sizeof(identifiers)/sizeof(identifiers[0]);i++)
  if(classify(identifiers[i],1))return 2;
 puts("PASS: Compiler 2 reserves exactly the 36 Swan keywords");
 return 0;
}
