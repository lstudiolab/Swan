#include "brew/codegen_x86_64_ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv){
    if(argc!=3)return 2;
    BrewIROp op=BREW_IR_SDIV;
    BrewIRType type=BREW_IR_I64;
    int64_t left=9,right=0;
    if(!strcmp(argv[1],"overflow")){
        left=INT64_MIN;
        right=-1;
    }else if(!strcmp(argv[1],"unsigned")){
        op=BREW_IR_UDIV;
    }else if(!strcmp(argv[1],"i32-zero")){
        type=BREW_IR_I32;
    }else if(!strcmp(argv[1],"i32-overflow")){
        type=BREW_IR_I32;
        left=INT32_MIN;
        right=-1;
    }else if(!strcmp(argv[1],"i32-unsigned")){
        type=BREW_IR_I32;
        op=BREW_IR_UDIV;
    }else if(strcmp(argv[1],"zero"))return 2;

    BrewIRModule module={0};
    BrewIRFunction *fn=brew_ir_add_function(&module,"main",type,NULL,0);
    BrewIRBlock *block=fn?brew_ir_add_block(fn,"entry"):NULL;
    if(!block)return 3;
    BrewIRInst constant={0};
    constant.op=BREW_IR_CONST;constant.type=type;constant.immediate=left;
    uint32_t a=brew_ir_add_inst(fn,block,constant);
    constant.immediate=right;
    uint32_t b=brew_ir_add_inst(fn,block,constant);
    BrewIRInst divide={0};
    divide.op=op;divide.type=type;divide.a=a;divide.b=b;
    uint32_t result=brew_ir_add_inst(fn,block,divide);
    if(!a||!b||!result||!brew_ir_terminate(block,(BrewIRTerm){BREW_IR_RET,result,0,0})){
        brew_ir_destroy(&module);return 4;
    }
    Text assembly={0};char error[256]={0};
    if(!brew_codegen_x86_64_ir(&module,&assembly,error,sizeof(error))){
        fprintf(stderr,"%s\\n",error);brew_ir_destroy(&module);return 5;
    }
    FILE *file=fopen(argv[2],"wb");
    bool ok=file&&fwrite(assembly.data,1,assembly.len,file)==assembly.len;
    if(file&&fclose(file)!=0)ok=false;
    free(assembly.data);brew_ir_destroy(&module);
    return ok?0:6;
}
