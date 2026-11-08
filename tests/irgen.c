#include "swan/lexer.h"
#include "swan/sema.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include "swan/svm.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const char *source="fn main() -> int:\n    let base = 6\n    return base * 7\n";
    SwanError error={0}; TokenStream tokens={0}; Program program={0};
    assert(swan_lex(source,&tokens,&error));
    assert(swan_parse(&tokens,&program,&error));
    assert(swan_semantic_check(&program,&error));
    SwanIRModule module={0};
    assert(swan_irgen_program(&program,&module,&error));
    char verify[256]={0};
    assert(swan_ir_optimize(&module,verify,sizeof(verify)));
    int64_t result=0;
    assert(swan_svm_run(&module,"main",1000,&result,verify,sizeof(verify)));
    assert(result==42);
    swan_ir_destroy(&module);
    swan_program_destroy(&program);
    free(tokens.items); free(tokens.text_storage);
    puts("PASS: .swan source -> typed IR -> optimized SVM execution");
    return 0;
}
