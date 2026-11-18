#include "swan/codegen_x86_64_ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc,char **argv){
    if(argc!=3)return 2;
    SwanIROp op=SWAN_IR_SDIV;
    SwanIRType type=SWAN_IR_I64;
    int64_t left=9,right=0;
    if(!strcmp(argv[1],"overflow")){
        left=INT64_MIN;
        right=-1;
    }else if(!strcmp(argv[1],"unsigned")){
        op=SWAN_IR_UDIV;
    }else if(!strcmp(argv[1],"i32-zero")){
        type=SWAN_IR_I32;
    }else if(!strcmp(argv[1],"i32-overflow")){
        type=SWAN_IR_I32;
        left=INT32_MIN;
        right=-1;
    }else if(!strcmp(argv[1],"i32-unsigned")){
        type=SWAN_IR_I32;
        op=SWAN_IR_UDIV;
    }else if(strcmp(argv[1],"zero"))return 2;

    SwanIRModule module={0};
    SwanIRFunction *fn=swan_ir_add_function(&module,"main",type,NULL,0);
    SwanIRBlock *block=fn?swan_ir_add_block(fn,"entry"):NULL;
    if(!block)return 3;
    SwanIRInst constant={0};
    constant.op=SWAN_IR_CONST;constant.type=type;constant.immediate=left;
    uint32_t a=swan_ir_add_inst(fn,block,constant);
    constant.immediate=right;
    uint32_t b=swan_ir_add_inst(fn,block,constant);
    SwanIRInst divide={0};
    divide.op=op;divide.type=type;divide.a=a;divide.b=b;
    uint32_t result=swan_ir_add_inst(fn,block,divide);
    if(!a||!b||!result||!swan_ir_terminate(block,(SwanIRTerm){SWAN_IR_RET,result,0,0})){
        swan_ir_destroy(&module);return 4;
    }
    Text assembly={0};char error[256]={0};
    if(!swan_codegen_x86_64_ir(&module,&assembly,error,sizeof(error))){
        fprintf(stderr,"%s\\n",error);swan_ir_destroy(&module);return 5;
    }
    FILE *file=fopen(argv[2],"wb");
    bool ok=file&&fwrite(assembly.data,1,assembly.len,file)==assembly.len;
    if(file&&fclose(file)!=0)ok=false;
    free(assembly.data);swan_ir_destroy(&module);
    return ok?0:6;
}
