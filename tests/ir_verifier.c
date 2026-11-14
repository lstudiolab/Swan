#include "swan/optimization.h"
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
    call.symbol="identity";call.incoming=malloc(sizeof(*call.incoming));
    if(!call.incoming)return 14;
    call.incoming[0]=(SwanIRIncoming){1,0};call.incoming_count=1;
    uint32_t result=swan_ir_add_inst(main_fn,entry,call);
    if(!result||!swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_RET,result,0,0}))return 13;
    int status=expect_valid(&m,"valid scalar call");
    swan_ir_destroy(&m);
    return status;
}
static int rejects_bad_call_signature(void){
    SwanIRModule m={0};
    SwanIRFunction *callee=swan_ir_add_function(&m,"identity",SWAN_IR_I32,
        (SwanIRType[]){SWAN_IR_I32},1);
    SwanIRFunction *caller=swan_ir_add_function(&m,"main",SWAN_IR_I32,NULL,0);
    if(!callee||!caller)return 15;
    callee->external=true;
    SwanIRBlock *entry=swan_ir_add_block(caller,"entry");
    if(!entry)return 16;
    SwanIRInst constant={0};constant.op=SWAN_IR_CONST;
    constant.type=SWAN_IR_I64;constant.immediate=42;
    uint32_t argument=swan_ir_add_inst(caller,entry,constant);
    SwanIRInst call={0};call.op=SWAN_IR_CALL;call.type=SWAN_IR_I32;
    call.symbol="identity";call.incoming=malloc(sizeof(*call.incoming));
    if(!call.incoming)return 17;
    call.incoming[0]=(SwanIRIncoming){argument,0};call.incoming_count=1;
    uint32_t result=swan_ir_add_inst(caller,entry,call);
    if(!result||!swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_RET,result,0,0}))return 18;
    int status=expect_invalid(&m,"call argument type mismatch","call argument type mismatch");
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
    phi.incoming=malloc(sizeof(*phi.incoming));
    if(!phi.incoming)return 33;
    phi.incoming[0]=(SwanIRIncoming){incoming,0};phi.incoming_count=1;
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
static int builder_guards(void){
    SwanIRModule m={0};
    if(swan_ir_add_function(NULL,"bad",SWAN_IR_I32,NULL,0))return 70;
    if(swan_ir_add_function(&m,"bad",SWAN_IR_I32,NULL,1))return 71;
    if(swan_ir_add_function(&m,"",SWAN_IR_I32,NULL,0))return 72;
    if(swan_ir_add_function(&m,"void_parameter",SWAN_IR_I32,(SwanIRType[]){SWAN_IR_VOID},1))return 73;
    if(swan_ir_add_block(NULL,"bad"))return 74;
    if(swan_ir_add_inst(NULL,NULL,(SwanIRInst){0}))return 75;
    SwanIRFunction *owner=swan_ir_add_function(&m,"owner",SWAN_IR_VOID,NULL,0);
    SwanIRFunction *other=swan_ir_add_function(&m,"other",SWAN_IR_VOID,NULL,0);
    if(!owner||!other)return 76;
    SwanIRBlock *foreign=swan_ir_add_block(owner,"foreign");
    if(!foreign||swan_ir_add_inst(other,foreign,(SwanIRInst){0}))return 77;
    void *storage=NULL;size_t capacity=0;
    if(swan_ir_reserve(&storage,&capacity,SIZE_MAX,2))return 78;
    if(storage||capacity)return 79;
    swan_ir_destroy(NULL);
    swan_ir_destroy(&m);
    return 0;
}
static int accepts_stale_ssa_location_cache(void){
    SwanIRModule module = {0};
    SwanIRFunction *function =
        swan_ir_add_function(&module, "main", SWAN_IR_I32, NULL, 0);
    SwanIRBlock *entry =
        function ? swan_ir_add_block(function, "entry") : NULL;
    if (entry == NULL) {
        return 90;
    }

    SwanIRInst constant = {0};
    constant.op = SWAN_IR_CONST;
    constant.type = SWAN_IR_I32;
    constant.immediate = 42;

    uint32_t value = swan_ir_add_inst(function, entry, constant);
    if (value == 0) {
        swan_ir_destroy(&module);
        return 91;
    }

    /* Simulate a transformation that moved the definition without rebuilding
     * the optional location cache. Verification must use the safe fallback. */
    function->value_locations[value] =
        (SwanIRValueLocation) {UINT32_MAX, UINT32_MAX};

    if (!swan_ir_terminate(entry, (SwanIRTerm) {SWAN_IR_RET, value, 0, 0})) {
        swan_ir_destroy(&module);
        return 92;
    }

    int status = expect_valid(&module, "stale SSA location cache");
    swan_ir_destroy(&module);
    return status;
}

static int rejects_duplicate_ssa_definitions(void){
    SwanIRModule m={0};
    SwanIRFunction *function=swan_ir_add_function(&m,"main",SWAN_IR_I32,NULL,0);
    SwanIRBlock *entry=function?swan_ir_add_block(function,"entry"):NULL;
    if(!entry)return 80;

    SwanIRInst constant={0};
    constant.op=SWAN_IR_CONST;
    constant.type=SWAN_IR_I32;
    constant.immediate=1;
    uint32_t first=swan_ir_add_inst(function,entry,constant);
    constant.immediate=2;
    uint32_t second=swan_ir_add_inst(function,entry,constant);
    if(!first||!second)return 81;

    /* Simulate malformed IR produced by a broken transformation pass. */
    entry->instructions[1].result=first;
    if(!swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_RET,first,0,0}))return 82;

    char error[256] = {0};
    if (swan_ir_verify(&m, error, sizeof(error))) {
        fprintf(stderr, "duplicate SSA definitions: malformed IR was accepted\n");
        swan_ir_destroy(&m);
        return 83;
    }
    if (!strstr(error, "duplicate SSA value ID") ||
        !strstr(error, "block 0, instruction 1")) {
        fprintf(stderr, "duplicate SSA definitions: imprecise diagnostic: %s\n", error);
        swan_ir_destroy(&m);
        return 84;
    }

    swan_ir_destroy(&m);
    return 0;
}


static int optimizer_preserves_boolean_constants(void){
    SwanIRModule module={0};
    SwanIRFunction *function=swan_ir_add_function(&module,"main",SWAN_IR_I1,NULL,0);
    SwanIRBlock *entry=function?swan_ir_add_block(function,"entry"):NULL;
    if(!entry){swan_ir_destroy(&module);return 95;}

    SwanIRInst truth={0};
    truth.op=SWAN_IR_CONST;
    truth.type=SWAN_IR_I1;
    truth.immediate=1;
    uint32_t value=swan_ir_add_inst(function,entry,truth);
    if(!value||!swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_RET,value,0,0})){
        swan_ir_destroy(&module);
        return 96;
    }

    char error[256]={0};
    if(!swan_ir_optimize(&module,error,sizeof(error))){
        fprintf(stderr,"boolean constant folding rejected true: %s\\n",error);
        swan_ir_destroy(&module);
        return 97;
    }
    if(swan_ir_value_type_of(function,value)!=SWAN_IR_I1||
       function->blocks[0].instructions[0].op!=SWAN_IR_CONST||
       function->blocks[0].instructions[0].immediate!=1){
        fprintf(stderr,"boolean constant folding did not preserve canonical true\\n");
        swan_ir_destroy(&module);
        return 98;
    }
    swan_ir_destroy(&module);
    return 0;
}

static int optimizer_repairs_compacted_block_metadata(void){
    SwanIRModule module={0};
    SwanIRFunction *function=swan_ir_add_function(&module,"main",SWAN_IR_I32,NULL,0);
    SwanIRBlock *entry=function?swan_ir_add_block(function,"entry"):NULL;
    SwanIRBlock *live=function?swan_ir_add_block(function,"live"):NULL;
    SwanIRBlock *dead=function?swan_ir_add_block(function,"dead"):NULL;
    if(!entry||!live||!dead){swan_ir_destroy(&module);return 100;}

    SwanIRInst condition={0};
    condition.op=SWAN_IR_CONST;
    condition.type=SWAN_IR_I1;
    condition.immediate=1;
    uint32_t condition_value=swan_ir_add_inst(function,entry,condition);

    SwanIRInst result={0};
    result.op=SWAN_IR_CONST;
    result.type=SWAN_IR_I32;
    result.immediate=42;
    uint32_t return_value=swan_ir_add_inst(function,entry,result);

    if(!condition_value||!return_value||
       !swan_ir_terminate(entry,(SwanIRTerm){SWAN_IR_COND_BR,condition_value,1,2})||
       !swan_ir_terminate(live,(SwanIRTerm){SWAN_IR_RET,return_value,0,0})||
       !swan_ir_terminate(dead,(SwanIRTerm){SWAN_IR_UNREACHABLE,0,0,0})){
        swan_ir_destroy(&module);
        return 101;
    }

    char error[256]={0};
    if(!swan_ir_optimize(&module,error,sizeof(error))){
        fprintf(stderr,"optimizer metadata repair: %s\n",error);
        swan_ir_destroy(&module);
        return 102;
    }

    if(function->block_count!=2||function->blocks[0].owner!=function||
       function->blocks[1].owner!=function||function->blocks[0].index!=0||
       function->blocks[1].index!=1){
        fprintf(stderr,"optimizer metadata repair: compacted block ownership/index is stale\n");
        swan_ir_destroy(&module);
        return 103;
    }

    SwanIRValueLocation location=function->value_locations[return_value];
    if(location.block!=0||location.index>=function->blocks[0].instruction_count||
       function->blocks[location.block].instructions[location.index].result!=return_value){
        fprintf(stderr,"optimizer metadata repair: SSA location cache is stale\n");
        swan_ir_destroy(&module);
        return 104;
    }

    int status=expect_valid(&module,"optimized compacted CFG");
    swan_ir_destroy(&module);
    return status;
}


static int repairs_block_owners_after_function_array_growth(void){
    SwanIRModule module={0};
    SwanIRFunction *first=swan_ir_add_function(&module,"first",SWAN_IR_VOID,NULL,0);
    if(!first)return 110;
    if(!swan_ir_add_block(first,"entry"))return 111;
    static const char *function_names[]={
        "function_0","function_1","function_2","function_3",
        "function_4","function_5","function_6","function_7"
    };
    for(size_t i=0;i<sizeof(function_names)/sizeof(function_names[0]);i++){
        if(!swan_ir_add_function(&module,function_names[i],SWAN_IR_VOID,NULL,0)){
            swan_ir_destroy(&module);
            return 112;
        }
    }

    /* Adding the ninth function grows the module array and moves its entries. */
    first=&module.functions[0];
    SwanIRBlock *entry=&first->blocks[0];
    SwanIRInst constant={0};
    constant.op=SWAN_IR_CONST;
    constant.type=SWAN_IR_I32;
    constant.immediate=7;
    uint32_t value=swan_ir_add_inst(first,entry,constant);
    if(!value){
        fprintf(stderr,"function-array growth left an IR block owner dangling\n");
        swan_ir_destroy(&module);
        return 113;
    }

    swan_ir_destroy(&module);
    return 0;
}

int main(void){
    int status=builder_guards();if(status)return status;
    status=valid_scalar_and_call();if(status)return status;
    status=rejects_bad_call_signature();if(status)return status;
    status=rejects_float_arithmetic();if(status)return status;
    status=rejects_late_phi();if(status)return status;
    status=rejects_unused_load_operand();if(status)return status;
    status=rejects_pointer_constant();if(status)return status;
    status=rejects_bad_terminator();if(status)return status;
    status=accepts_stale_ssa_location_cache();if(status)return status;
    status=rejects_duplicate_ssa_definitions();if(status)return status;
    status=optimizer_preserves_boolean_constants();if(status)return status;
    status=optimizer_repairs_compacted_block_metadata();if(status)return status;
    status=repairs_block_owners_after_function_array_growth();if(status)return status;
    puts("Swan typed IR verifier passed: valid calls and SSA accepted; duplicate definitions, invalid types, PHI placement, operand arity, pointer constants, and terminators rejected.");
    return 0;
}
