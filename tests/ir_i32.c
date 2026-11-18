#include "swan/codegen_x86_64_ir.h"
#include <stdio.h>
#include <stdlib.h>

static uint32_t constant(SwanIRFunction *fn,SwanIRBlock *block,
                         SwanIRType type,int64_t value){
    SwanIRInst in={0};in.op=SWAN_IR_CONST;in.type=type;in.immediate=value;
    return swan_ir_add_inst(fn,block,in);
}
static uint32_t binary(SwanIRFunction *fn,SwanIRBlock *block,SwanIROp op,
                       SwanIRType type,uint32_t left,uint32_t right){
    SwanIRInst in={0};in.op=op;in.type=type;in.a=left;in.b=right;
    return swan_ir_add_inst(fn,block,in);
}
int main(int argc,char **argv){
    if(argc!=2)return 2;
    SwanIRModule module={0};
    const SwanIRType parameters[]={SWAN_IR_I32,SWAN_IR_I32};
    if(!swan_ir_add_function(&module,"add32",SWAN_IR_I32,parameters,2)||
       !swan_ir_add_function(&module,"main",SWAN_IR_I32,NULL,0))return 3;
    SwanIRFunction *add=&module.functions[0],*main_fn=&module.functions[1];
    SwanIRBlock *add_block=swan_ir_add_block(add,"entry");
    SwanIRBlock *main_block=swan_ir_add_block(main_fn,"entry");
    if(!add_block||!main_block)return 4;

    uint32_t sum=binary(add,add_block,SWAN_IR_ADD,SWAN_IR_I32,1,2);
    uint32_t three=constant(add,add_block,SWAN_IR_I32,3);
    uint32_t product=binary(add,add_block,SWAN_IR_MUL,SWAN_IR_I32,sum,three);
    uint32_t two=constant(add,add_block,SWAN_IR_I32,2);
    uint32_t difference=binary(add,add_block,SWAN_IR_SUB,SWAN_IR_I32,product,two);
    uint32_t divisor=constant(add,add_block,SWAN_IR_I32,3);
    uint32_t quotient=binary(add,add_block,SWAN_IR_SDIV,SWAN_IR_I32,difference,divisor);
    uint32_t remainder=binary(add,add_block,SWAN_IR_SREM,SWAN_IR_I32,difference,divisor);
    uint32_t total=binary(add,add_block,SWAN_IR_ADD,SWAN_IR_I32,quotient,remainder);
    if(!sum||!three||!product||!two||!difference||!divisor||!quotient||
       !remainder||!total||
       !swan_ir_terminate(add_block,(SwanIRTerm){SWAN_IR_RET,total,0,0}))return 5;

    uint32_t left=constant(main_fn,main_block,SWAN_IR_I32,-7);
    uint32_t right=constant(main_fn,main_block,SWAN_IR_I32,2);
    SwanIRInst call={0};call.op=SWAN_IR_CALL;call.type=SWAN_IR_I32;call.symbol="add32";
    call.incoming=calloc(2,sizeof(*call.incoming));
    if(!call.incoming)return 6;
    call.incoming_count=2;
    call.incoming[0].value=left;call.incoming[1].value=right;
    uint32_t called=swan_ir_add_inst(main_fn,main_block,call);
    if(!called){free(call.incoming);return 7;}

    uint32_t one=constant(main_fn,main_block,SWAN_IR_I32,1);
    uint32_t five=constant(main_fn,main_block,SWAN_IR_I32,5);
    uint32_t shifted=binary(main_fn,main_block,SWAN_IR_SHL,SWAN_IR_I32,one,five);
    uint32_t three_main=constant(main_fn,main_block,SWAN_IR_I32,3);
    uint32_t masked=binary(main_fn,main_block,SWAN_IR_OR,SWAN_IR_I32,shifted,three_main);
    uint32_t combined=binary(main_fn,main_block,SWAN_IR_ADD,SWAN_IR_I32,called,masked);

    uint32_t seventeen=constant(main_fn,main_block,SWAN_IR_I32,17);
    uint32_t five_main=constant(main_fn,main_block,SWAN_IR_I32,5);
    uint32_t unsigned_q=binary(main_fn,main_block,SWAN_IR_UDIV,SWAN_IR_I32,seventeen,five_main);
    uint32_t unsigned_r=binary(main_fn,main_block,SWAN_IR_UREM,SWAN_IR_I32,seventeen,five_main);
    uint32_t unsigned_sum=binary(main_fn,main_block,SWAN_IR_ADD,SWAN_IR_I32,unsigned_q,unsigned_r);
    uint32_t result=binary(main_fn,main_block,SWAN_IR_ADD,SWAN_IR_I32,combined,unsigned_sum);
    if(!left||!right||!one||!five||!shifted||!three_main||!masked||!combined||
       !seventeen||!five_main||!unsigned_q||!unsigned_r||!unsigned_sum||!result||
       !swan_ir_terminate(main_block,(SwanIRTerm){SWAN_IR_RET,result,0,0}))return 8;

    Text assembly={0};char error[256]={0};
    if(!swan_codegen_x86_64_ir(&module,&assembly,error,sizeof(error))){
        fprintf(stderr,"x86-64 i32 lowering failed: %s\n",error);
        swan_ir_destroy(&module);return 9;
    }
    FILE *output=fopen(argv[1],"wb");
    bool ok=output&&fwrite(assembly.data,1,assembly.len,output)==assembly.len;
    if(output&&fclose(output)!=0)ok=false;
    free(assembly.data);swan_ir_destroy(&module);
    return ok?0:10;
}
