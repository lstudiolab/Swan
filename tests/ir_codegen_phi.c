#include "brew/codegen_x86_64_ir.h"
#include "brew/codegen_aarch64.h"
#include "brew/codegen_wasm64.h"
#include "brew/optimization.h"
#include <stdio.h>
#include <stdlib.h>

static uint32_t add_constant(BrewIRFunction *function,BrewIRBlock *block,
                             BrewIRType type,int64_t value){
    BrewIRInst instruction={0};
    instruction.op=BREW_IR_CONST;
    instruction.type=type;
    instruction.immediate=value;
    return brew_ir_add_inst(function,block,instruction);
}

static uint32_t add_phi(BrewIRFunction *function,BrewIRBlock *block,
                        uint32_t first,uint32_t first_block,
                        uint32_t second,uint32_t second_block){
    BrewIRInst instruction={0};
    instruction.op=BREW_IR_PHI;
    instruction.type=BREW_IR_I64;
    instruction.incoming=malloc(2*sizeof(*instruction.incoming));
    if(!instruction.incoming)return 0;
    instruction.incoming[0]=(BrewIRIncoming){first,first_block};
    instruction.incoming[1]=(BrewIRIncoming){second,second_block};
    instruction.incoming_count=2;
    uint32_t result=brew_ir_add_inst(function,block,instruction);
    if(!result)free(instruction.incoming);
    return result;
}

int main(int argc,char **argv){
    if(argc!=2)return 2;
    BrewIRModule module={0};
    BrewIRFunction *function=brew_ir_add_function(&module,"main",BREW_IR_I64,NULL,0);
    BrewIRBlock *entry=function?brew_ir_add_block(function,"entry"):NULL;
    BrewIRBlock *header=function?brew_ir_add_block(function,"header"):NULL;
    BrewIRBlock *body=function?brew_ir_add_block(function,"body"):NULL;
    BrewIRBlock *exit_block=function?brew_ir_add_block(function,"exit"):NULL;
    if(!entry||!header||!body||!exit_block)return 3;

    uint32_t zero=add_constant(function,entry,BREW_IR_I64,0);
    uint32_t one=add_constant(function,entry,BREW_IR_I64,1);
    uint32_t initial_x=add_constant(function,entry,BREW_IR_I64,1);
    uint32_t initial_y=add_constant(function,entry,BREW_IR_I64,2);
    if(!zero||!one||!initial_x||!initial_y||
       !brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_BR,0,1,0}))return 4;

    uint32_t iteration=add_phi(function,header,zero,0,0,2);
    uint32_t x=add_phi(function,header,initial_x,0,0,2);
    uint32_t y=add_phi(function,header,initial_y,0,0,2);
    if(!iteration||!x||!y)return 5;
    header->instructions[0].incoming[1].value=iteration;
    header->instructions[1].incoming[1].value=y;
    header->instructions[2].incoming[1].value=x;

    BrewIRInst compare={0};
    compare.op=BREW_IR_ICMP_SLT;
    compare.type=BREW_IR_I1;
    compare.a=iteration;
    compare.b=one;
    uint32_t condition=brew_ir_add_inst(function,header,compare);
    if(!condition||
       !brew_ir_terminate(header,(BrewIRTerm){BREW_IR_COND_BR,condition,2,3}))return 6;

    BrewIRInst increment={0};
    increment.op=BREW_IR_ADD;
    increment.type=BREW_IR_I64;
    increment.a=iteration;
    increment.b=one;
    uint32_t next_iteration=brew_ir_add_inst(function,body,increment);
    if(!next_iteration)return 7;
    header->instructions[0].incoming[1].value=next_iteration;
    if(!brew_ir_terminate(body,(BrewIRTerm){BREW_IR_BR,0,1,0}))return 7;

    uint32_t ten=add_constant(function,exit_block,BREW_IR_I64,10);
    BrewIRInst multiply={0};
    multiply.op=BREW_IR_MUL;
    multiply.type=BREW_IR_I64;
    multiply.a=x;
    multiply.b=ten;
    uint32_t tens=brew_ir_add_inst(function,exit_block,multiply);
    BrewIRInst add={0};
    add.op=BREW_IR_ADD;
    add.type=BREW_IR_I64;
    add.a=tens;
    add.b=y;
    uint32_t result=brew_ir_add_inst(function,exit_block,add);
    if(!ten||!tens||!result||
       !brew_ir_terminate(exit_block,(BrewIRTerm){BREW_IR_RET,result,0,0}))return 8;

    char error[256]={0};
    if(!brew_ir_optimize(&module,error,sizeof(error))){
        fprintf(stderr,"IR optimization failed: %s\n",error);
        brew_ir_destroy(&module);
        return 9;
    }
    Text assembly={0};
    if(!brew_codegen_x86_64_ir(&module,&assembly,error,sizeof(error))){
        fprintf(stderr,"x86-64 IR code generation failed: %s\n",error);
        free(assembly.data);
        brew_ir_destroy(&module);
        return 10;
    }
    Text aarch64={0};
    if(!brew_codegen_aarch64_ir(&module,&aarch64,error,sizeof(error))){
        fprintf(stderr,"AArch64 IR code generation failed: %s\n",error);
        free(aarch64.data);
        free(assembly.data);
        brew_ir_destroy(&module);
        return 11;
    }
    if(!strstr(aarch64.data,".Lsw_e")||!strstr(aarch64.data,"str x9, [sp, #")){
        fprintf(stderr,"AArch64 phi edge-copy sequence was not emitted\n");
        free(aarch64.data);
        free(assembly.data);
        brew_ir_destroy(&module);
        return 12;
    }
    BrewWasmBuffer wasm={0};
    if(!brew_codegen_wasm64(&module,&wasm,error,sizeof(error))||wasm.length<8){
        fprintf(stderr,"WebAssembly phi lowering failed: %s\n",error);
        free(wasm.data);
        free(aarch64.data);
        free(assembly.data);
        brew_ir_destroy(&module);
        return 15;
    }
    FILE *output=fopen(argv[1],"wb");
    bool written=output&&fwrite(assembly.data,1,assembly.len,output)==assembly.len;
    if(output&&fclose(output)!=0)written=false;
    free(wasm.data);
    free(aarch64.data);
    free(assembly.data);
    brew_ir_destroy(&module);
    return written?0:16;
}
