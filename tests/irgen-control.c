#include "swan/lexer.h"
#include "swan/sema.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include "swan/svm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int64_t execute(const char *source) {
    SwanError error={0};TokenStream tokens={0};Program program={0};
    assert(swan_lex(source,&tokens,&error));
    if(!swan_parse(&tokens,&program,&error)){fprintf(stderr,"parse failed at %u:%u: %s\n",error.line,error.col,error.message);abort();}
    assert(swan_semantic_check(&program,&error));
    SwanIRModule module={0};assert(swan_irgen_program(&program,&module,&error));
    char detail[256]={0};assert(swan_ir_optimize(&module,detail,sizeof(detail)));
    int64_t result=0;assert(swan_svm_run(&module,"main",100000,&result,detail,sizeof(detail)));
    swan_ir_destroy(&module);swan_program_destroy(&program);
    free(tokens.items);free(tokens.text_storage);return result;
}
int main(void) {
    const char *loop=
        "fn sum(n: int) -> int:\n"
        "    var total: int = 0\n"
        "    var i: int = 0\n"
        "    while i < n:\n"
        "        total = total + i\n"
        "        i = i + 1\n"
        "    out\n"
        "    return total\n"
        "out\n"
        "fn main() -> int:\n"
        "    return sum(10)\n"
        "out\n";
    assert(execute(loop)==45);
    const char *branch=
        "fn main() -> int:\n"
        "    var x: int = 7\n"
        "    if x > 3:\n"
        "        x = x * 6\n"
        "    out\n"
        "    else:\n"
        "        x = 1\n"
        "    out\n"
        "    return x\n"
        "out\n";
    assert(execute(branch)==42);
    puts("PASS: IR lowering handles calls, mutable locals, loops and branches");
    return 0;
}
