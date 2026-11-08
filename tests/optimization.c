#include "swan/optimization.h"
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
 puts("PASS: verified optimization pass manager, constant folding and dead-code elimination");
 return 0;
}
