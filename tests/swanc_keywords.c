#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
extern int64_t swan_fn_swan_lex(uint32_t *, int64_t, uint32_t *, int64_t, uint32_t *);

static const char *const keywords[] = {
 "func","struct","import","export","enum","let","var","const","if","else",
 "while","loop","true","false","guard","defer","return","break","continue",
 "block","as","bool","from","upto","inline","static","private","public",
 "match","mod","in","int","init","deinit","self","pass"
};
static const char *const identifiers[] = {
 "fn","out","store","manual","for","type","null","nil","none","namespace",
 "variant","protocol","thread","async","external","lazy","case","set",
 "print","free","throw","rethrow","borrow","and","or","not","extern",
 "functional","letx","Func"
};
static int run(const char *const *words,size_t count,int expected_keyword) {
 uint32_t source[4096]={0},tokens[4096]={0},diagnostic[4]={0};
 size_t length=0;
 for(size_t i=0;i<count;i++) {
  if(i)source[length++]=' ';
  for(const unsigned char *p=(const unsigned char*)words[i];*p;p++)source[length++]=*p;
 }
 int64_t n=swan_fn_swan_lex(source,(int64_t)length,tokens,4096,diagnostic);
 if(n!=(int64_t)count+1) {
  fprintf(stderr,"lexer returned %lld tokens, expected %zu (diagnostic %u)\n",(long long)n,count+1,diagnostic[0]);
  return 1;
 }
 for(size_t i=0;i<count;i++) {
  uint32_t kind=tokens[i*5];
  if(expected_keyword ? kind==1 : kind!=1) {
   fprintf(stderr,"%s classified incorrectly: token kind %u\n",words[i],kind);
   return 2;
  }
 }
 return 0;
}
int main(void) {
 if(sizeof(keywords)/sizeof(keywords[0])!=36)return 3;
 if(run(keywords,36,1))return 1;
 if(run(identifiers,sizeof(identifiers)/sizeof(identifiers[0]),0))return 2;
 puts("PASS: Compiler 2 lexer reserves exactly 36 Swan keywords");
 return 0;
}
