#include "swan/codegen_x86_64_ir.h"
#include "swan/codegen_aarch64.h"
#include "swan/codegen_riscv64.h"
#include "swan/optimization.h"
#include <stdio.h>
#include <stdlib.h>

static uint32_t add_constant(SwanIRFunction *function,SwanIRBlock *block,
                             SwanIRType type,int64_t value){
    SwanIRInst instruction={0};
    instruction.op=SWAN_IR_CONST;
    instruction.type=type;
    instruction.immediate=value;
    return swan_ir_add_inst(function,block,instruction);
}

static uint32_t add_phi(SwanIRFunction *function,SwanIRBlock *block,
                        uint32_t first,uint32_t first_block,
                        uint32_t second,uint32_t second_block){
    SwanIRInst instruction={0};
    instruction.op=SWAN_IR_PHI;
    instruction.type=SWAN_IR_I64;
    instruction.incoming=malloc(2*sizeof(*instruction.incoming));
    if(!instruction.incoming)return 0;
    instruction.incoming[0]=(SwanIRIncoming){first,first_block};
    instruction.incoming[1]=(SwanIRIncoming){second,second_block};
    instruction.incoming_count=2;
    uint32_t result=swan_ir_add_inst(function,block,instruction);
    if(!result)free(instruction.incoming);
    return result;
}

int main(int argc,char **argv){
    if(argc!=2)return 2;
    SwanIRModule module={0};
    SwanIRFunction *function=swan_ir_add_function(&module,"main",SWAN_IR_I64,NULL,0);
    SwanIRBlock *entry=function?swan_ir_add_block(function,"entry"):NULL;
    SwanIRBlock *header=function?swan_ir_add_block(function,"header"):NULL;
    SwanIRBlock *body=function?swan_ir_add_block(function,"body"):NULL;
    SwanIRBlock *exit_block=function?swan_ir_add_block(function,"exit"):NULL;
    if(!entry||!header||!body||!exit_block)return 3;

    uint32_t zero=add_constant(function,entry,SWAN_IR_I64,0);
    uint32_t one=add_constant(function,entry,SWAN_IR_I64,1);
    uint32_t initial_x=add_constant(function,entry,SWAN_IR_I64,1);
    uint32_t initial_y=add_constant(function,entry,SWAN_IR_I64,2);
    if(!zero||!one||!initial_x||!initial_y||
       !swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_BR,0,1,0}))return 4;

    uint32_t iteration=add_phi(function,header,zero,0,0,2);
    uint32_t x=add_phi(function,header,initial_x,0,0,2);
    uint32_t y=add_phi(function,header,initial_y,0,0,2);
    if(!iteration||!x||!y)return 5;
    header->instructions[0].incoming[1].value=iteration;
    header->instructions[1].incoming[1].value=y;
    header->instructions[2].incoming[1].value=x;

    SwanIRInst compare={0};
    compare.op=SWAN_IR_ICMP_SLT;
    compare.type=SWAN_IR_I1;
    compare.a=iteration;
    compare.b=one;
    uint32_t condition=swan_ir_add_inst(function,header,compare);
    if(!condition||
       !swan_ir_terminate(header,(SwanIRTerm){SWAN_IR_COND_BR,condition,2,3}))return 6;

    SwanIRInst increment={0};
    increment.op=SWAN_IR_ADD;
    increment.type=SWAN_IR_I64;
    increment.a=iteration;
    increment.b=one;
    uint32_t next_iteration=swan_ir_add_inst(function,body,increment);
    if(!next_iteration)return 7;
    header->instructions[0].incoming[1].value=next_iteration;
    if(!swan_ir_terminate(body,(SwanIRTerm){SWAN_IR_BR,0,1,0}))return 7;

    uint32_t ten=add_constant(function,exit_block,SWAN_IR_I64,10);
    SwanIRInst multiply={0};
    multiply.op=SWAN_IR_MUL;
    multiply.type=SWAN_IR_I64;
    multiply.a=x;
    multiply.b=ten;
    uint32_t tens=swan_ir_add_inst(function,exit_block,multiply);
    SwanIRInst add={0};
    add.op=SWAN_IR_ADD;
    add.type=SWAN_IR_I64;
    add.a=tens;
    add.b=y;
    uint32_t result=swan_ir_add_inst(function,exit_block,add);
    if(!ten||!tens||!result||
       !swan_ir_terminate(exit_block,(SwanIRTerm){SWAN_IR_RET,result,0,0}))return 8;

    char error[256]={0};
    if(!swan_ir_optimize(&module,error,sizeof(error))){
        fprintf(stderr,"IR optimization failed: %s\n",error);
        swan_ir_destroy(&module);
        return 9;
    }
    Text assembly={0};
    if(!swan_codegen_x86_64_ir(&module,&assembly,error,sizeof(error))){
        fprintf(stderr,"x86-64 IR code generation failed: %s\n",error);
        free(assembly.data);
        swan_ir_destroy(&module);
        return 10;
    }
    Text aarch64={0};
    if(!swan_codegen_aarch64_ir(&module,&aarch64,error,sizeof(error))){
        fprintf(stderr,"AArch64 IR code generation failed: %s\n",error);
        free(aarch64.data);
        free(assembly.data);
        swan_ir_destroy(&module);
        return 11;
    }
    if(!strstr(aarch64.data,".Lsw_e")||!strstr(aarch64.data,"str x9, [sp, #")){
        fprintf(stderr,"AArch64 phi edge-copy sequence was not emitted\n");
        free(aarch64.data);
        free(assembly.data);
        swan_ir_destroy(&module);
        return 12;
    }
    Text riscv={0};
    if(!swan_codegen_riscv64_ir(&module,&riscv,error,sizeof(error))){
        fprintf(stderr,"RISC-V64 IR code generation failed: %s\n",error);
        free(riscv.data);
        free(aarch64.data);
        free(assembly.data);
        swan_ir_destroy(&module);
        return 13;
    }
    if(!strstr(riscv.data,".Lsw_e")||!strstr(riscv.data,"sd t0, ")){
        fprintf(stderr,"RISC-V64 phi edge-copy sequence was not emitted\n");
        free(riscv.data);
        free(aarch64.data);
        free(assembly.data);
        swan_ir_destroy(&module);
        return 14;
    }
    FILE *output=fopen(argv[1],"wb");
    bool written=output&&fwrite(assembly.data,1,assembly.len,output)==assembly.len;
    if(output&&fclose(output)!=0)written=false;
    free(riscv.data);
    free(aarch64.data);
    free(assembly.data);
    swan_ir_destroy(&module);
    return written?0:15;
}
