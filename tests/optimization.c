#include "swan/optimization.h"\n#include "swan/svm.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 SwanIRModule m={0};SwanIRFunction *f=swan_ir_add_function(&m,"main",SWAN_IR_I64,NULL,0);assert(f);
 SwanIRBlock *b=swan_ir_add_block(f,"entry");assert(b);
 SwanIRInst a={0};a.op=SWAN_IR_CONST;a.type=SWAN_IR_I64;a.immediate=20;uint32_t av=swan_ir_add_inst(f,b,a);
 SwanIRInst x={0};x.op=SWAN_IR_CONST;x.type=SWAN_IR_I64;x.immediate=22;uint32_t xv=swan_ir_add_inst(f,b,x);
 SwanIRInst unused={0};unused.op=SWAN_IR_CONST;unused.type=SWAN_IR_I64;unused.immediate=999;assert(swan_ir_add_inst(f,b,unused));
 SwanIRInst sum={0};sum.op=SWAN_IR_ADD;sum.type=SWAN_IR_I64;sum.a=av;sum.b=xv;uint32_t result=swan_ir_add_inst(f,b,sum);
 assert(swan_ir_terminate(b,(SwanIRTerm){SWAN_IR_RET,result,0,0}));
 char error[256]={0};assert(swan_ir_optimize(&m,error,sizeof(error)));
 assert(b->instruction_count==1);
 assert(b->instructions[0].op==SWAN_IR_CONST&&b->instructions[0].immediate==42);
 assert(swan_ir_verify(&m,error,sizeof(error)));swan_ir_destroy(&m);
 SwanIRModule cse={0};SwanIRType cparams[]={SWAN_IR_I64,SWAN_IR_I64};SwanIRFunction *cf=swan_ir_add_function(&cse,"cse",SWAN_IR_I64,cparams,2);assert(cf);
 SwanIRBlock *cb=swan_ir_add_block(cf,"entry");assert(cb);
 SwanIRInst add1={0};add1.op=SWAN_IR_ADD;add1.type=SWAN_IR_I64;add1.a=1;add1.b=2;uint32_t sum1=swan_ir_add_inst(cf,cb,add1);
 SwanIRInst add2=add1;uint32_t sum2=swan_ir_add_inst(cf,cb,add2);
 SwanIRInst mul={0};mul.op=SWAN_IR_MUL;mul.type=SWAN_IR_I64;mul.a=sum2;mul.b=1;uint32_t product=swan_ir_add_inst(cf,cb,mul);
 assert(swan_ir_terminate(cb,(SwanIRTerm){SWAN_IR_RET,product,0,0}));
 assert(swan_ir_optimize(&cse,error,sizeof(error)));assert(swan_ir_verify(&cse,error,sizeof(error)));assert(cb->instruction_count==2);assert(cb->instructions[0].op==SWAN_IR_ADD);assert(cb->instructions[1].op==SWAN_IR_MUL&&cb->instructions[1].a==sum1);
 swan_ir_destroy(&cse);
 SwanIRModule cfg={0};SwanIRFunction *gf=swan_ir_add_function(&cfg,"main",SWAN_IR_I64,NULL,0);assert(gf);
 SwanIRBlock *entry=swan_ir_add_block(gf,"entry"),*yes=swan_ir_add_block(gf,"yes"),*no=swan_ir_add_block(gf,"no");assert(entry&&yes&&no);
 SwanIRInst condition={0};condition.op=SWAN_IR_CONST;condition.type=SWAN_IR_I1;condition.immediate=0;uint32_t cv=swan_ir_add_inst(gf,entry,condition);
 SwanIRInst one={0};one.op=SWAN_IR_CONST;one.type=SWAN_IR_I64;one.immediate=1;uint32_t ov=swan_ir_add_inst(gf,yes,one);
 SwanIRInst two={0};two.op=SWAN_IR_CONST;two.type=SWAN_IR_I64;two.immediate=2;uint32_t tv=swan_ir_add_inst(gf,no,two);
 assert(swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_COND_BR,cv,1,2}));
 assert(swan_ir_terminate(yes,(SwanIRTerm){SWAN_IR_RET,ov,0,0}));
 assert(swan_ir_terminate(no,(SwanIRTerm){SWAN_IR_RET,tv,0,0}));
 assert(swan_ir_optimize(&cfg,error,sizeof(error)));assert(gf->block_count==2);
 assert(swan_svm_run(&cfg,"main",100,&folded,error,sizeof(error))&&folded==2);
 swan_ir_destroy(&cfg);
 puts("PASS: verified optimization pass manager, constant folding, algebraic simplification, common-subexpression elimination and dead-code elimination");
 return 0;
}
