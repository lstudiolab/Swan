#include "swan/ir.h"
#include <assert.h>
int main(void) {
 SwanIRModule m={0};SwanIRType params[]={SWAN_IR_I64};
 SwanIRFunction *f=swan_ir_add_function(&m,"sum",SWAN_IR_I64,params,1);assert(f);
 SwanIRBlock *entry=swan_ir_add_block(f,"entry");assert(entry);
 SwanIRInst add={0};add.op=SWAN_IR_ADD;add.type=SWAN_IR_I64;add.a=1;add.b=1;
 uint32_t result=swan_ir_add_inst(f,entry,add);assert(result==2);
 SwanIRTerm ret={SWAN_IR_RET,result,0,0};assert(swan_ir_terminate(entry,ret));
 char error[256];assert(swan_ir_verify(&m,error,sizeof(error)));
 entry->terminator.value=99;assert(!swan_ir_verify(&m,error,sizeof(error)));
 assert(strstr(error,"return uses undefined value"));
 entry->terminator.value=result;assert(swan_ir_verify(&m,error,sizeof(error)));
 swan_ir_destroy(&m);return 0;
}
