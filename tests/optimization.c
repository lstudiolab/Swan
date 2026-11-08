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
 SwanIRInst c0={0};c0.op=SWAN_IR_CONST;c0.type=SWAN_IR_I64;c0.immediate=7;uint32_t v0=swan_ir_add_inst(cf,cb,c0);
 SwanIRInst c1={0};c1.op=SWAN_IR_CONST;c1.type=SWAN_IR_I64;c1.immediate=9;uint32_t v1=swan_ir_add_inst(cf,cb,c1);
 SwanIRInst add1={0};add1.op=SWAN_IR_ADD;add1.type=SWAN_IR_I64;add1.a=v0;add1.b=v1;uint32_t sum1=swan_ir_add_inst(cf,cb,add1);
 SwanIRInst add2=add1;uint32_t sum2=swan_ir_add_inst(cf,cb,add2);
 SwanIRInst mul={0};mul.op=SWAN_IR_MUL;mul.type=SWAN_IR_I64;mul.a=sum2;mul.b=v0;uint32_t product=swan_ir_add_inst(cf,cb,mul);
 assert(swan_ir_terminate(cb,(SwanIRTerm){SWAN_IR_RET,product,0,0}));
 assert(swan_ir_optimize(&cse,error,sizeof(error)));assert(swan_ir_verify(&cse,error,sizeof(error)));assert(cb->instruction_count==1);assert(cb->instructions[0].op==SWAN_IR_MUL&&cb->instructions[0].a==sum1);
 swan_ir_destroy(&cse);
 puts("PASS: verified optimization pass manager, constant folding, algebraic simplification, common-subexpression elimination and dead-code elimination");
 return 0;
}
