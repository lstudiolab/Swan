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
    FILE *artifact=tmpfile(); assert(artifact);
    assert(swan_svm_write_module(artifact,&module));
    rewind(artifact);
    SwanIRModule loaded={0};
    assert(swan_svm_read_module(artifact,&loaded,error,sizeof(error)));
    fclose(artifact);
    assert(swan_svm_run(&loaded,"main",100,&result,error,sizeof(error)));
    assert(result==42);
    FILE *truncated=tmpfile();assert(truncated);
    assert(swan_svm_write_bytes(truncated,"SWANVM01",8));
    assert(swan_svm_write_u32(truncated,SWAN_SVM_VERSION));
    assert(swan_svm_write_u32(truncated,0));
    assert(swan_svm_write_u64(truncated,4));
    rewind(truncated);SwanIRModule rejected={0};
    assert(!swan_svm_read_module(truncated,&rejected,error,sizeof(error)));
    fclose(truncated);
    FILE *trailing=tmpfile();assert(trailing);
    assert(swan_svm_write_bytes(trailing,"SWANVM01",8));
    assert(swan_svm_write_u32(trailing,SWAN_SVM_VERSION));
    assert(swan_svm_write_u32(trailing,0));
    assert(swan_svm_write_u64(trailing,4));
    assert(swan_svm_write_u32(trailing,0));
    assert(swan_svm_write_u8(trailing,0xff));
    rewind(trailing);assert(!swan_svm_read_module(trailing,&rejected,error,sizeof(error)));
    fclose(trailing);
    swan_ir_destroy(&loaded);
    swan_ir_destroy(&module);
    SwanIRModule narrow={0};
    SwanIRFunction *small=swan_ir_add_function(&narrow,"small",SWAN_IR_I8,NULL,0);assert(small);
    SwanIRBlock *sb=swan_ir_add_block(small,"entry");assert(sb);
    SwanIRInst ca={0};ca.op=SWAN_IR_CONST;ca.type=SWAN_IR_I8;ca.immediate=127;uint32_t cav=swan_ir_add_inst(small,sb,ca);
    SwanIRInst cb={0};cb.op=SWAN_IR_CONST;cb.type=SWAN_IR_I8;cb.immediate=1;uint32_t cbv=swan_ir_add_inst(small,sb,cb);
    SwanIRInst plus={0};plus.op=SWAN_IR_ADD;plus.type=SWAN_IR_I8;plus.a=cav;plus.b=cbv;uint32_t sum=swan_ir_add_inst(small,sb,plus);
    assert(swan_ir_terminate(sb,(SwanIRTerm){SWAN_IR_RET,sum,0,0}));
    assert(swan_svm_run(&narrow,"small",100,&result,error,sizeof(error))&&result==-128);
    assert(swan_ir_optimize(&narrow,error,sizeof(error)));
    assert(swan_svm_run(&narrow,"small",100,&result,error,sizeof(error))&&result==-128);
    swan_ir_destroy(&narrow);
    puts("PASS: SVM executes and optimizes portable typed IR");
    return 0;
}
