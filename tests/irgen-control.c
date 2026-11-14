#include "swan/lexer.h"
#include "swan/sema.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void compile_and_verify(const char *source) {
    SwanError error={0};TokenStream tokens={0};Program program={0};
    assert(swan_lex(source,&tokens,&error));
    assert(swan_parse(&tokens,&program,&error));
    assert(swan_semantic_check(&program,&error));
    SwanIRModule module={0};assert(swan_irgen_program(&program,&module,&error));
    char detail[256]={0};assert(swan_ir_optimize(&module,detail,sizeof(detail)));
    assert(swan_ir_verify(&module,detail,sizeof(detail)));
    swan_ir_destroy(&module);swan_program_destroy(&program);
    free(tokens.items);free(tokens.text_storage);
}
int main(void) {
    const char *loop=
        "func sum(n: int) -> int:\n"
        "    var total: int = 0\n"
        "    var i: int = 0\n"
        "    while i < n:\n"
        "        total = total + i\n"
        "        i = i + 1\n"
        "    return total\n"
        "func main() -> int:\n"
        "    return sum(10)\n"
        ;
    compile_and_verify(loop);
    const char *branch=
        "func main() -> int:\n"
        "    var x: int = 7\n"
        "    if x > 3:\n"
        "        x = x * 6\n"
        "    else:\n"
        "        x = 1\n"
        "    return x\n"
        ;
    compile_and_verify(branch);
    puts("PASS: IR lowering handles calls, mutable locals, loops and branches");
    return 0;
}
