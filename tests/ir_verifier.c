#include "swan/ir.h"
#include <stdio.h>
#include <string.h>

static int expect_valid(SwanIRModule *m,const char *name){
    char error[256]={0};
    if(!swan_ir_verify(m,error,sizeof(error))){
        fprintf(stderr,"%s: expected valid IR, got: %s\n",name,error);
        return 1;
    }
    return 0;
}
static int expect_invalid(SwanIRModule *m,const char *name,const char *needle){
    char error[256]={0};
    if(swan_ir_verify(m,error,sizeof(error))){
        fprintf(stderr,"%s: malformed IR was accepted\n",name);
        return 1;
    }
    if(needle&&!strstr(error,needle)){
        fprintf(stderr,"%s: wrong diagnostic: %s\n",name,error);
        return 2;
    }
    return 0;
}
static int valid_scalar_and_call(void){
    SwanIRModule m={0};
    SwanIRFunction *callee=swan_ir_add_function(&m,"identity",SWAN_IR_I32,
        (SwanIRType[]){SWAN_IR_I32},1);
    if(!callee)return 10;
    callee->external=true;
    SwanIRFunction *main_fn=swan_ir_add_function(&m,"main",SWAN_IR_I32,
        (SwanIRType[]){SWAN_IR_I32},1);
    if(!main_fn)return 11;
    SwanIRBlock *entry=swan_ir_add_block(main_fn,"entry");
    if(!entry)return 12;
    SwanIRInst call={0};call.op=SWAN_IR_CALL;call.type=SWAN_IR_I32;
    call.symbol="identity";call.incoming=(SwanIRIncoming[]){{1,0}};
    call.incoming_count=1;
    uint32_t result=swan_ir_add_inst(main_fn,entry,call);
    if(!result||!swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_RET,result,0,0}))return 13;
    int status=expect_valid(&m,"valid scalar call");
    swan_ir_destroy(&m);
    return status;
}
static int rejects_float_arithmetic(void){
    SwanIRModule m={0};SwanIRFunction *f=swan_ir_add_function(&m,"main",SWAN_IR_F32,NULL,0);
    SwanIRBlock *b=f?swan_ir_add_block(f,"entry"):NULL;
    if(!b)return 20;
    SwanIRInst c={0};c.op=SWAN_IR_CONST;c.type=SWAN_IR_F32;c.immediate=0x3f800000;
    uint32_t a=swan_ir_add_inst(f,b,c);c.immediate=0x40000000;
    uint32_t z=swan_ir_add_inst(f,b,c);
    SwanIRInst add={0};add.op=SWAN_IR_ADD;add.type=SWAN_IR_F32;add.a=a;add.b=z;
    uint32_t value=swan_ir_add_inst(f,b,add);
    if(!value||!swan_ir_terminate(b,(SwanIRTerm){SWAN_IR_RET,value,0,0}))return 21;
    int status=expect_invalid(&m,"floating-point arithmetic","integer operands");
    swan_ir_destroy(&m);
    return status;
}
static int rejects_late_phi(void){
    SwanIRModule m={0};SwanIRFunction *f=swan_ir_add_function(&m,"main",SWAN_IR_I32,NULL,0);
    SwanIRBlock *entry=f?swan_ir_add_block(f,"entry"):NULL;
    SwanIRBlock *merge=f?swan_ir_add_block(f,"merge"):NULL;
    if(!entry||!merge)return 30;
    SwanIRInst c={0};c.op=SWAN_IR_CONST;c.type=SWAN_IR_I32;c.immediate=7;
    uint32_t incoming=swan_ir_add_inst(f,entry,c);
    if(!swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_BR,0,1,0}))return 31;
    c.immediate=9;uint32_t ordinary=swan_ir_add_inst(f,merge,c);
    SwanIRInst phi={0};phi.op=SWAN_IR_PHI;phi.type=SWAN_IR_I32;
    phi.incoming=(SwanIRIncoming[]){{incoming,0}};phi.incoming_count=1;
    uint32_t result=swan_ir_add_inst(f,merge,phi);
    if(!ordinary||!result||!swan_ir_terminate(merge,(SwanIRTerm){SWAN_IR_RET,result,0,0}))return 32;
    int status=expect_invalid(&m,"late phi","phi instructions must precede");
    swan_ir_destroy(&m);
    return status;
}
static int rejects_unused_load_operand(void){
    SwanIRModule m={0};SwanIRFunction *f=swan_ir_add_function(&m,"main",SWAN_IR_I32,NULL,0);
    SwanIRBlock *b=f?swan_ir_add_block(f,"entry"):NULL;
    if(!b)return 40;
    SwanIRInst alloc={0};alloc.op=SWAN_IR_ALLOCA;alloc.type=SWAN_IR_PTR;
    uint32_t ptr=swan_ir_add_inst(f,b,alloc);
    SwanIRInst c={0};c.op=SWAN_IR_CONST;c.type=SWAN_IR_I32;c.immediate=4;
    uint32_t extra=swan_ir_add_inst(f,b,c);
    SwanIRInst load={0};load.op=SWAN_IR_LOAD;load.type=SWAN_IR_I32;load.a=ptr;load.b=extra;
    uint32_t result=swan_ir_add_inst(f,b,load);
    if(!result||!swan_ir_terminate(b,(SwanIRTerm){SWAN_IR_RET,result,0,0}))return 41;
    int status=expect_invalid(&m,"load with extra operand","load has an invalid result or unused operands");
    swan_ir_destroy(&m);
    return status;
}
static int rejects_pointer_constant(void){
    SwanIRModule m={0};SwanIRFunction *f=swan_ir_add_function(&m,"main",SWAN_IR_PTR,NULL,0);
    SwanIRBlock *b=f?swan_ir_add_block(f,"entry"):NULL;
    if(!b)return 50;
    SwanIRInst c={0};c.op=SWAN_IR_CONST;c.type=SWAN_IR_PTR;c.immediate=0;
    uint32_t result=swan_ir_add_inst(f,b,c);
    if(!result||!swan_ir_terminate(b,(SwanIRTerm){SWAN_IR_RET,result,0,0}))return 51;
    int status=expect_invalid(&m,"pointer constant","constant has an invalid type");
    swan_ir_destroy(&m);
    return status;
}
static int rejects_bad_terminator(void){
    SwanIRModule m={0};SwanIRFunction *f=swan_ir_add_function(&m,"main",SWAN_IR_VOID,NULL,0);
    SwanIRBlock *b=f?swan_ir_add_block(f,"entry"):NULL;
    if(!b)return 60;
    if(!swan_ir_terminate(b,(SwanIRTerm){SWAN_IR_BR,0,0,1}))return 61;
    int status=expect_invalid(&m,"branch with unused alternate","unconditional branch has unused operands");
    swan_ir_destroy(&m);
    return status;
}
int main(void){
    int status=valid_scalar_and_call();if(status)return status;
    status=rejects_float_arithmetic();if(status)return status;
    status=rejects_late_phi();if(status)return status;
    status=rejects_unused_load_operand();if(status)return status;
    status=rejects_pointer_constant();if(status)return status;
    status=rejects_bad_terminator();if(status)return status;
    puts("Swan typed IR verifier passed: valid calls and SSA accepted; invalid types, PHI placement, operand arity, pointer constants, and terminators rejected.");
    return 0;
}
