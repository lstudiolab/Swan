#include "swan/lexer.h"
#include "swan/sema.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const char *source=
        "fn explode() -> bool:\n"
        "    return 1 / 0 == 0\n"
        "out\n"
        "fn double(x: int) -> int:\n"
        "    return x * 2\n"
        "out\n"
        "fn main() -> int:\n"
        "    var i = 0\n"
        "    var total = 0\n"
        "    while i < 10:\n"
        "        i = i + 1\n"
        "        if i == 3:\n"
        "            continue\n"
        "        out\n"
        "        if i == 8:\n"
        "            break\n"
        "        out\n"
        "        total = total + double(i)\n"
        "    out\n"
        "    if false and explode():\n"
        "        return 1\n"
        "    out\n"
        "    if true or explode():\n"
        "        return total\n"
        "    out\n"
        "    return 0\n"
        "out\n";
    SwanError error={0}; TokenStream tokens={0}; Program program={0};
    assert(swan_lex(source,&tokens,&error));
    assert(swan_parse(&tokens,&program,&error));
    assert(swan_semantic_check(&program,&error));
    SwanIRModule module={0};
    assert(swan_irgen_program(&program,&module,&error));
    char detail[256]={0};
    assert(swan_ir_optimize(&module,detail,sizeof(detail)));
    assert(swan_ir_verify(&module,detail,sizeof(detail)));
    swan_ir_destroy(&module);
    swan_program_destroy(&program);free(tokens.items);free(tokens.text_storage);
    puts("PASS: .swan -> typed IR -> verified optimization, loops, branches and calls");
    return 0;
}
