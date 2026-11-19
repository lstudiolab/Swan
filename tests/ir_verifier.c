#include "brew/optimization.h"
#include <stdio.h>
#include <string.h>

static int expect_valid(BrewIRModule *m,const char *name){
    char error[256]={0};
    if(!brew_ir_verify(m,error,sizeof(error))){
        fprintf(stderr,"%s: expected valid IR, got: %s\n",name,error);
        return 1;
    }
    return 0;
}
static int expect_invalid(BrewIRModule *m,const char *name,const char *needle){
    char error[256]={0};
    if(brew_ir_verify(m,error,sizeof(error))){
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
    BrewIRModule m={0};
    BrewIRFunction *main_fn=brew_ir_add_function(&m,"main",BREW_IR_I32,
        (BrewIRType[]){BREW_IR_I32},1);
    if(!main_fn)return 11;
    /* Add the callee after its caller to exercise indexed forward lookup. */
    BrewIRFunction *callee=brew_ir_add_function(&m,"identity",BREW_IR_I32,
        (BrewIRType[]){BREW_IR_I32},1);
    if(!callee)return 10;
    callee->external=true;
    BrewIRBlock *entry=brew_ir_add_block(main_fn,"entry");
    if(!entry)return 12;
    BrewIRInst call={0};call.op=BREW_IR_CALL;call.type=BREW_IR_I32;
    call.symbol="identity";call.incoming=malloc(sizeof(*call.incoming));
    if(!call.incoming)return 14;
    call.incoming[0]=(BrewIRIncoming){1,0};call.incoming_count=1;
    uint32_t result=brew_ir_add_inst(main_fn,entry,call);
    if(!result||!brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,result,0,0}))return 13;
    int status=expect_valid(&m,"valid scalar call");
    brew_ir_destroy(&m);
    return status;
}
static int rejects_bad_call_signature(void){
    BrewIRModule m={0};
    BrewIRFunction *callee=brew_ir_add_function(&m,"identity",BREW_IR_I32,
        (BrewIRType[]){BREW_IR_I32},1);
    BrewIRFunction *caller=brew_ir_add_function(&m,"main",BREW_IR_I32,NULL,0);
    if(!callee||!caller)return 15;
    callee->external=true;
    BrewIRBlock *entry=brew_ir_add_block(caller,"entry");
    if(!entry)return 16;
    BrewIRInst constant={0};constant.op=BREW_IR_CONST;
    constant.type=BREW_IR_I64;constant.immediate=42;
    uint32_t argument=brew_ir_add_inst(caller,entry,constant);
    BrewIRInst call={0};call.op=BREW_IR_CALL;call.type=BREW_IR_I32;
    call.symbol="identity";call.incoming=malloc(sizeof(*call.incoming));
    if(!call.incoming)return 17;
    call.incoming[0]=(BrewIRIncoming){argument,0};call.incoming_count=1;
    uint32_t result=brew_ir_add_inst(caller,entry,call);
    if(!result||!brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,result,0,0}))return 18;
    int status=expect_invalid(&m,"call argument type mismatch","call argument type mismatch");
    brew_ir_destroy(&m);
    return status;
}
static int rejects_float_arithmetic(void){
    BrewIRModule m={0};BrewIRFunction *f=brew_ir_add_function(&m,"main",BREW_IR_F32,NULL,0);
    BrewIRBlock *b=f?brew_ir_add_block(f,"entry"):NULL;
    if(!b)return 20;
    BrewIRInst c={0};c.op=BREW_IR_CONST;c.type=BREW_IR_F32;c.immediate=0x3f800000;
    uint32_t a=brew_ir_add_inst(f,b,c);c.immediate=0x40000000;
    uint32_t z=brew_ir_add_inst(f,b,c);
    BrewIRInst add={0};add.op=BREW_IR_ADD;add.type=BREW_IR_F32;add.a=a;add.b=z;
    uint32_t value=brew_ir_add_inst(f,b,add);
    if(!value||!brew_ir_terminate(b,(BrewIRTerm){BREW_IR_RET,value,0,0}))return 21;
    int status=expect_invalid(&m,"floating-point arithmetic","integer operands");
    brew_ir_destroy(&m);
    return status;
}
static int rejects_late_phi(void){
    BrewIRModule m={0};BrewIRFunction *f=brew_ir_add_function(&m,"main",BREW_IR_I32,NULL,0);
    BrewIRBlock *entry=f?brew_ir_add_block(f,"entry"):NULL;
    BrewIRBlock *merge=f?brew_ir_add_block(f,"merge"):NULL;
    if(!entry||!merge)return 30;
    BrewIRInst c={0};c.op=BREW_IR_CONST;c.type=BREW_IR_I32;c.immediate=7;
    uint32_t incoming=brew_ir_add_inst(f,entry,c);
    if(!brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_BR,0,1,0}))return 31;
    c.immediate=9;uint32_t ordinary=brew_ir_add_inst(f,merge,c);
    BrewIRInst phi={0};phi.op=BREW_IR_PHI;phi.type=BREW_IR_I32;
    phi.incoming=malloc(sizeof(*phi.incoming));
    if(!phi.incoming)return 33;
    phi.incoming[0]=(BrewIRIncoming){incoming,0};phi.incoming_count=1;
    uint32_t result=brew_ir_add_inst(f,merge,phi);
    if(!ordinary||!result||!brew_ir_terminate(merge,(BrewIRTerm){BREW_IR_RET,result,0,0}))return 32;
    int status=expect_invalid(&m,"late phi","phi instructions must precede");
    brew_ir_destroy(&m);
    return status;
}
static int rejects_unused_load_operand(void){
    BrewIRModule m={0};BrewIRFunction *f=brew_ir_add_function(&m,"main",BREW_IR_I32,NULL,0);
    BrewIRBlock *b=f?brew_ir_add_block(f,"entry"):NULL;
    if(!b)return 40;
    BrewIRInst alloc={0};alloc.op=BREW_IR_ALLOCA;alloc.type=BREW_IR_PTR;
    uint32_t ptr=brew_ir_add_inst(f,b,alloc);
    BrewIRInst c={0};c.op=BREW_IR_CONST;c.type=BREW_IR_I32;c.immediate=4;
    uint32_t extra=brew_ir_add_inst(f,b,c);
    BrewIRInst load={0};load.op=BREW_IR_LOAD;load.type=BREW_IR_I32;load.a=ptr;load.b=extra;
    uint32_t result=brew_ir_add_inst(f,b,load);
    if(!result||!brew_ir_terminate(b,(BrewIRTerm){BREW_IR_RET,result,0,0}))return 41;
    int status=expect_invalid(&m,"load with extra operand","load has an invalid result or unused operands");
    brew_ir_destroy(&m);
    return status;
}
static int rejects_pointer_constant(void){
    BrewIRModule m={0};BrewIRFunction *f=brew_ir_add_function(&m,"main",BREW_IR_PTR,NULL,0);
    BrewIRBlock *b=f?brew_ir_add_block(f,"entry"):NULL;
    if(!b)return 50;
    BrewIRInst c={0};c.op=BREW_IR_CONST;c.type=BREW_IR_PTR;c.immediate=0;
    uint32_t result=brew_ir_add_inst(f,b,c);
    if(!result||!brew_ir_terminate(b,(BrewIRTerm){BREW_IR_RET,result,0,0}))return 51;
    int status=expect_invalid(&m,"pointer constant","constant has an invalid type");
    brew_ir_destroy(&m);
    return status;
}
static int rejects_bad_terminator(void){
    BrewIRModule m={0};BrewIRFunction *f=brew_ir_add_function(&m,"main",BREW_IR_VOID,NULL,0);
    BrewIRBlock *b=f?brew_ir_add_block(f,"entry"):NULL;
    if(!b)return 60;
    if(!brew_ir_terminate(b,(BrewIRTerm){BREW_IR_BR,0,0,1}))return 61;
    int status=expect_invalid(&m,"branch with unused alternate","unconditional branch has unused operands");
    brew_ir_destroy(&m);
    return status;
}
static int builder_guards(void){
    BrewIRModule m={0};
    if(brew_ir_add_function(NULL,"bad",BREW_IR_I32,NULL,0))return 70;
    if(brew_ir_add_function(&m,"bad",BREW_IR_I32,NULL,1))return 71;
    if(brew_ir_add_function(&m,"",BREW_IR_I32,NULL,0))return 72;
    if(brew_ir_add_function(&m,"void_parameter",BREW_IR_I32,(BrewIRType[]){BREW_IR_VOID},1))return 73;
    if(brew_ir_add_block(NULL,"bad"))return 74;
    if(brew_ir_add_inst(NULL,NULL,(BrewIRInst){0}))return 75;
    BrewIRFunction *owner=brew_ir_add_function(&m,"owner",BREW_IR_VOID,NULL,0);
    BrewIRFunction *other=brew_ir_add_function(&m,"other",BREW_IR_VOID,NULL,0);
    if(!owner||!other)return 76;
    BrewIRBlock *foreign=brew_ir_add_block(owner,"foreign");
    if(!foreign||brew_ir_add_inst(other,foreign,(BrewIRInst){0}))return 77;
    void *storage=NULL;size_t capacity=0;
    if(brew_ir_reserve(&storage,&capacity,SIZE_MAX,2))return 78;
    if(storage||capacity)return 79;
    brew_ir_destroy(NULL);
    brew_ir_destroy(&m);
    return 0;
}
static int accepts_stale_ssa_location_cache(void){
    BrewIRModule module = {0};
    BrewIRFunction *function =
        brew_ir_add_function(&module, "main", BREW_IR_I32, NULL, 0);
    BrewIRBlock *entry =
        function ? brew_ir_add_block(function, "entry") : NULL;
    if (entry == NULL) {
        return 90;
    }

    BrewIRInst constant = {0};
    constant.op = BREW_IR_CONST;
    constant.type = BREW_IR_I32;
    constant.immediate = 42;

    uint32_t value = brew_ir_add_inst(function, entry, constant);
    if (value == 0) {
        brew_ir_destroy(&module);
        return 91;
    }

    /* Simulate a transformation that moved the definition without rebuilding
     * the optional location cache. Verification must use the safe fallback. */
    function->value_locations[value] =
        (BrewIRValueLocation) {UINT32_MAX, UINT32_MAX};

    if (!brew_ir_terminate(entry, (BrewIRTerm) {BREW_IR_RET, value, 0, 0})) {
        brew_ir_destroy(&module);
        return 92;
    }

    int status = expect_valid(&module, "stale SSA location cache");
    brew_ir_destroy(&module);
    return status;
}

static int rejects_duplicate_ssa_definitions(void){
    BrewIRModule m={0};
    BrewIRFunction *function=brew_ir_add_function(&m,"main",BREW_IR_I32,NULL,0);
    BrewIRBlock *entry=function?brew_ir_add_block(function,"entry"):NULL;
    if(!entry)return 80;

    BrewIRInst constant={0};
    constant.op=BREW_IR_CONST;
    constant.type=BREW_IR_I32;
    constant.immediate=1;
    uint32_t first=brew_ir_add_inst(function,entry,constant);
    constant.immediate=2;
    uint32_t second=brew_ir_add_inst(function,entry,constant);
    if(!first||!second)return 81;

    /* Simulate malformed IR produced by a broken transformation pass. */
    entry->instructions[1].result=first;
    if(!brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,first,0,0}))return 82;

    char error[256] = {0};
    if (brew_ir_verify(&m, error, sizeof(error))) {
        fprintf(stderr, "duplicate SSA definitions: malformed IR was accepted\n");
        brew_ir_destroy(&m);
        return 83;
    }
    if (!strstr(error, "duplicate SSA value ID") ||
        !strstr(error, "block 0, instruction 1")) {
        fprintf(stderr, "duplicate SSA definitions: imprecise diagnostic: %s\n", error);
        brew_ir_destroy(&m);
        return 84;
    }

    brew_ir_destroy(&m);
    return 0;
}


static int optimizer_preserves_boolean_constants(void){
    BrewIRModule module={0};
    BrewIRFunction *function=brew_ir_add_function(&module,"main",BREW_IR_I1,NULL,0);
    BrewIRBlock *entry=function?brew_ir_add_block(function,"entry"):NULL;
    if(!entry){brew_ir_destroy(&module);return 95;}

    BrewIRInst truth={0};
    truth.op=BREW_IR_CONST;
    truth.type=BREW_IR_I1;
    truth.immediate=1;
    uint32_t value=brew_ir_add_inst(function,entry,truth);
    if(!value||!brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,value,0,0})){
        brew_ir_destroy(&module);
        return 96;
    }

    char error[256]={0};
    if(!brew_ir_optimize(&module,error,sizeof(error))){
        fprintf(stderr,"boolean constant folding rejected true: %s\\n",error);
        brew_ir_destroy(&module);
        return 97;
    }
    if(brew_ir_value_type_of(function,value)!=BREW_IR_I1||
       function->blocks[0].instructions[0].op!=BREW_IR_CONST||
       function->blocks[0].instructions[0].immediate!=1){
        fprintf(stderr,"boolean constant folding did not preserve canonical true\\n");
        brew_ir_destroy(&module);
        return 98;
    }
    brew_ir_destroy(&module);
    return 0;
}

static int optimizer_repairs_compacted_block_metadata(void){
    BrewIRModule module={0};
    BrewIRFunction *function=brew_ir_add_function(&module,"main",BREW_IR_I32,NULL,0);
    BrewIRBlock *entry=function?brew_ir_add_block(function,"entry"):NULL;
    BrewIRBlock *live=function?brew_ir_add_block(function,"live"):NULL;
    BrewIRBlock *dead=function?brew_ir_add_block(function,"dead"):NULL;
    if(!entry||!live||!dead){brew_ir_destroy(&module);return 100;}

    BrewIRInst condition={0};
    condition.op=BREW_IR_CONST;
    condition.type=BREW_IR_I1;
    condition.immediate=1;
    uint32_t condition_value=brew_ir_add_inst(function,entry,condition);

    BrewIRInst result={0};
    result.op=BREW_IR_CONST;
    result.type=BREW_IR_I32;
    result.immediate=42;
    uint32_t return_value=brew_ir_add_inst(function,entry,result);

    if(!condition_value||!return_value||
       !brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_COND_BR,condition_value,1,2})||
       !brew_ir_terminate(live,(BrewIRTerm){BREW_IR_RET,return_value,0,0})||
       !brew_ir_terminate(dead,(BrewIRTerm){BREW_IR_UNREACHABLE,0,0,0})){
        brew_ir_destroy(&module);
        return 101;
    }

    char error[256]={0};
    if(!brew_ir_optimize(&module,error,sizeof(error))){
        fprintf(stderr,"optimizer metadata repair: %s\n",error);
        brew_ir_destroy(&module);
        return 102;
    }

    if(function->block_count!=2||function->blocks[0].owner!=function||
       function->blocks[1].owner!=function||function->blocks[0].index!=0||
       function->blocks[1].index!=1){
        fprintf(stderr,"optimizer metadata repair: compacted block ownership/index is stale\n");
        brew_ir_destroy(&module);
        return 103;
    }

    BrewIRValueLocation location=function->value_locations[return_value];
    if(location.block!=0||location.index>=function->blocks[0].instruction_count||
       function->blocks[location.block].instructions[location.index].result!=return_value){
        fprintf(stderr,"optimizer metadata repair: SSA location cache is stale\n");
        brew_ir_destroy(&module);
        return 104;
    }

    int status=expect_valid(&module,"optimized compacted CFG");
    brew_ir_destroy(&module);
    return status;
}


static int repairs_block_owners_after_function_array_growth(void){
    BrewIRModule module={0};
    BrewIRFunction *first=brew_ir_add_function(&module,"first",BREW_IR_VOID,NULL,0);
    if(!first)return 110;
    if(!brew_ir_add_block(first,"entry"))return 111;
    static const char *function_names[]={
        "function_0","function_1","function_2","function_3",
        "function_4","function_5","function_6","function_7"
    };
    for(size_t i=0;i<sizeof(function_names)/sizeof(function_names[0]);i++){
        if(!brew_ir_add_function(&module,function_names[i],BREW_IR_VOID,NULL,0)){
            brew_ir_destroy(&module);
            return 112;
        }
    }

    /* Adding the ninth function grows the module array and moves its entries. */
    first=&module.functions[0];
    BrewIRBlock *entry=&first->blocks[0];
    BrewIRInst constant={0};
    constant.op=BREW_IR_CONST;
    constant.type=BREW_IR_I32;
    constant.immediate=7;
    uint32_t value=brew_ir_add_inst(first,entry,constant);
    if(!value){
        fprintf(stderr,"function-array growth left an IR block owner dangling\n");
        brew_ir_destroy(&module);
        return 113;
    }

    brew_ir_destroy(&module);
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
    puts("Brew typed IR verifier passed: valid calls and SSA accepted; duplicate definitions, invalid types, PHI placement, operand arity, pointer constants, and terminators rejected.");
    return 0;
}
