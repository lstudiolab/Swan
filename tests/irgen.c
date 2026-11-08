#include "swan/lexer.h"
#include "swan/sema.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include "swan/svm.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const char *source=
        "fn explode() -> bool:\n"
        "    return 1 / 0 == 0\n"
        "fn double(x: int) -> int:\n"
        "    return x * 2\n"
        "fn main() -> int:\n"
        "    var i = 0\n"
        "    var total = 0\n"
        "    while i < 10:\n"
        "        i = i + 1\n"
        "        if i == 3:\n"
        "            continue\n"
        "        if i == 8:\n"
        "            break\n"
        "        total = total + double(i)\n"
        "    if false and explode():\n"
        "        return 1\n"
        "    if true or explode():\n"
        "        return total\n"
        "    return 0\n";
    SwanError error={0}; TokenStream tokens={0}; Program program={0};
    assert(swan_lex(source,&tokens,&error));
    assert(swan_parse(&tokens,&program,&error));
    assert(swan_semantic_check(&program,&error));
    SwanIRModule module={0};
    assert(swan_irgen_program(&program,&module,&error));
    char detail[256]={0};
    assert(swan_ir_optimize(&module,detail,sizeof(detail)));
    int64_t result=0;
    assert(swan_svm_run(&module,"main",10000,&result,detail,sizeof(detail)));
    assert(result==50);
    FILE *artifact=tmpfile();assert(artifact);
    assert(swan_svm_write_module(artifact,&module));rewind(artifact);
    SwanIRModule loaded={0};
    assert(swan_svm_read_module(artifact,&loaded,detail,sizeof(detail)));
    fclose(artifact);
    result=0;assert(swan_svm_run(&loaded,"main",10000,&result,detail,sizeof(detail)));
    assert(result==50);
    swan_ir_destroy(&loaded);swan_ir_destroy(&module);
    swan_program_destroy(&program);free(tokens.items);free(tokens.text_storage);
    puts("PASS: .swan -> typed IR -> optimized SVM, loops, branches, calls and module round-trip");
    return 0;
}
