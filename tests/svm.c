#include "swan/svm.h"
#include "swan/optimization.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    SwanIRModule module={0};
    SwanIRFunction *fn=swan_ir_add_function(&module,"main",SWAN_IR_I64,NULL,0);
    assert(fn);
    SwanIRBlock *entry=swan_ir_add_block(fn,"entry");
    assert(entry);
    SwanIRInst a={0}; a.op=SWAN_IR_CONST; a.type=SWAN_IR_I64; a.immediate=6;
    uint32_t av=swan_ir_add_inst(fn,entry,a); assert(av);
    SwanIRInst b={0}; b.op=SWAN_IR_CONST; b.type=SWAN_IR_I64; b.immediate=7;
    uint32_t bv=swan_ir_add_inst(fn,entry,b); assert(bv);
    SwanIRInst mul={0}; mul.op=SWAN_IR_MUL; mul.type=SWAN_IR_I64; mul.a=av; mul.b=bv;
    uint32_t product=swan_ir_add_inst(fn,entry,mul); assert(product);
    assert(swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_RET,product,0,0}));
    char error[256]={0}; int64_t result=0;
    assert(swan_svm_run(&module,"main",100,&result,error,sizeof(error)));
    assert(result==42);
    assert(swan_ir_optimize(&module,error,sizeof(error)));
    assert(swan_svm_run(&module,"main",100,&result,error,sizeof(error)));
    assert(result==42);
    swan_ir_destroy(&module);
    puts("PASS: SVM executes and optimizes portable typed IR");
    return 0;
}
