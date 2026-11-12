#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define IR_INSTRUCTIONS 515
#define CONSTANTS 256
#define ADDITIONS 128
#define ROUNDS 128
#define WORKSPACE_WORDS (3 * (IR_INSTRUCTIONS + 1) + 2)
extern int64_t swan_fn_swanc_ir_optimize(uint32_t *, int64_t, uint32_t *, int64_t);
extern int64_t swan_fn_swanc_ir_verify(uint32_t *, int64_t, int64_t, uint32_t *);
extern int64_t swan_fn_swanc_ir_constant_fold(uint32_t *, int64_t, uint32_t *, int64_t);
extern int64_t swan_fn_swanc_ir_dead_code_eliminate(uint32_t *, int64_t, uint32_t *, int64_t);

static void put(uint32_t *ir,uint32_t i,uint32_t op,uint32_t ty,uint32_t r,
                uint32_t a,uint32_t b,uint32_t c,uint32_t imm){
    uint32_t p=i*8; ir[p]=op;ir[p+1]=ty;ir[p+2]=r;ir[p+3]=a;
    ir[p+4]=b;ir[p+5]=c;ir[p+6]=imm;ir[p+7]=0;
}
static int workload(void){
    uint32_t ir[IR_INSTRUCTIONS*8],ws[WORKSPACE_WORDS];
    for(int round=0;round<ROUNDS;++round){
        memset(ir,0,sizeof(ir));memset(ws,0,sizeof(ws));
        put(ir,0,22,8,1,0,0,0,0);
        for(uint32_t i=0;i<CONSTANTS;++i)put(ir,i+1,0,4,i+2,0,0,0,i+1);
        for(uint32_t i=0;i<ADDITIONS;++i)put(ir,1+CONSTANTS+i,1,4,CONSTANTS+i+2,i*2+2,i*2+3,0,0);
        for(uint32_t i=0;i<ADDITIONS;++i)put(ir,1+CONSTANTS+ADDITIONS+i,24,0,0,1,CONSTANTS+i+2,0,0);
        uint32_t load=1+CONSTANTS+ADDITIONS*2,dead=load+1;
        put(ir,load,23,4,CONSTANTS+ADDITIONS+2,1,0,0,0);
        put(ir,dead,1,4,CONSTANTS+ADDITIONS+3,CONSTANTS+ADDITIONS+2,2,0,0);
        int64_t status=swan_fn_swanc_ir_optimize(ir,IR_INSTRUCTIONS,ws,IR_INSTRUCTIONS+1);
        if(status){fprintf(stderr,"optimizer failed: status=%lld diagnostic=%u at=%u\n",(long long)status,ws[0],ws[1]);return 1;}
        for(uint32_t i=0;i<ADDITIONS;++i){
            uint32_t p=(1+CONSTANTS+i)*8,expected=(i*2+1)+(i*2+2);
            if(ir[p]!=0||ir[p+6]!=expected){fprintf(stderr,"incorrect fold round=%d instruction=%u\n",round,i);return 2;}
            if(ir[(1+CONSTANTS+ADDITIONS+i)*8]!=24){fprintf(stderr,"DCE removed store\n");return 3;}
        }
        if(ir[dead*8]!=0)return 4;
        if(ir[load*8]!=23)return 5;
    }
    return 0;
}
static int constant_fold_overflow(void){
    uint32_t ir[14*8]={0},ws[3*15+2]={0};
    put(ir,0,22,8,1,0,0,0,0);
    put(ir,1,0,4,2,0,0,0,UINT32_MAX);
    put(ir,2,0,4,3,0,0,0,1);
    put(ir,3,1,4,4,2,3,0,0);
    put(ir,4,0,4,5,0,0,0,0);
    put(ir,5,2,4,6,5,3,0,0);
    put(ir,6,0,4,7,0,0,0,2);
    put(ir,7,3,4,8,2,7,0,0);
    put(ir,8,24,0,0,1,4,0,0);
    put(ir,9,24,0,0,1,6,0,0);
    put(ir,10,24,0,0,1,8,0,0);
    put(ir,11,0,4,9,0,0,0,UINT32_MAX-1u);
    put(ir,12,1,4,10,9,3,0,0);
    put(ir,13,24,0,0,1,10,0,0);
    fprintf(stderr,"overflow regression: verify\n");
    int64_t status=swan_fn_swanc_ir_verify(ir,14,0,ws);
    if(status){fprintf(stderr,"verify status=%lld diag=%u\n",(long long)status,ws[0]);return 40;}
    fprintf(stderr,"overflow regression: fold\n");
    status=swan_fn_swanc_ir_constant_fold(ir,14,ws,15);
    if(status){fprintf(stderr,"fold status=%lld\n",(long long)status);return 41;}
    fprintf(stderr,"overflow regression: dce\n");
    status=swan_fn_swanc_ir_dead_code_eliminate(ir,14,ws,15);
    if(status){fprintf(stderr,"dce status=%lld\n",(long long)status);return 42;}
    fprintf(stderr,"overflow regression: final verify\n");
    status=swan_fn_swanc_ir_verify(ir,14,0,ws);
    if(status){fprintf(stderr,"final verify status=%lld diag=%u\n",(long long)status,ws[0]);return 43;}
    if(ir[3*8]!=1||ir[5*8]!=2||ir[7*8]!=3){
        fprintf(stderr,"overflowing i32 operation was folded\n");
        return 44;
    }
    if(ir[12*8]!=0||ir[12*8+6]!=UINT32_MAX){
        fprintf(stderr,"valid UINT32_MAX addition was not folded\n");
        return 45;
    }
    return 0;
}
static int edge_cases(void){
    uint32_t ir[12*8]={0},ws[3*13+2]={0};
    put(ir,0,22,8,1,0,0,0,0);
    put(ir,1,0,4,2,0,0,0,10);put(ir,2,0,4,3,0,0,0,20);
    put(ir,3,1,4,4,2,3,0,0);put(ir,4,1,4,5,4,2,0,0);
    put(ir,5,24,0,0,1,4,0,0);put(ir,6,23,4,6,1,0,0,0);
    put(ir,7,4,4,7,2,3,0,0);put(ir,8,25,4,8,2,3,0,0);
    /* This chain depends on a load, so it cannot constant-fold. Reverse DCE
       must remove the leaf, then its now-unused producer, then the root. */
    put(ir,9,1,4,9,6,2,0,0);put(ir,10,1,4,10,9,6,0,0);
    put(ir,11,1,4,11,10,2,0,0);
    if(swan_fn_swanc_ir_optimize(ir,12,ws,13)){fprintf(stderr,"edge optimizer failed: %u at %u\n",ws[0],ws[1]);return 10;}
    if(ir[3*8]!=0||ir[3*8+6]!=30)return 11;
    if(ir[5*8]!=24||ir[6*8]!=23||ir[7*8]!=4||ir[8*8]!=25){fprintf(stderr,"side effect/trap/call changed\n");return 12;}
    if(ir[9*8]!=0||ir[10*8]!=0||ir[11*8]!=0){fprintf(stderr,"cascading DCE failed\n");return 13;}
    return 0;
}
static int expect_verifier_error(uint32_t *ir,int count,uint32_t expected,const char *name){
    uint32_t ws[3*17+2]={0};
    int64_t status=swan_fn_swanc_ir_optimize(ir,count,ws,count+1);
    if(status==0||ws[0]!=expected){
        fprintf(stderr,"%s: status=%lld diagnostic=%u expected=%u\n",
                name,(long long)status,ws[0],expected);
        return 1;
    }
    return 0;
}
static int verifier_contracts(void){
    uint32_t ir[16*8]={0};
    put(ir,0,0,6,1,0,0,0,0x3f800000);
    put(ir,1,0,6,2,0,0,0,0x40000000);
    put(ir,2,1,6,3,1,2,0,0);
    if(expect_verifier_error(ir,3,10,"floating-point arithmetic"))return 30;

    memset(ir,0,sizeof(ir));
    put(ir,0,0,4,1,0,0,0,4);put(ir,1,0,4,2,0,0,0,5);
    put(ir,2,1,5,3,1,2,0,0);
    if(expect_verifier_error(ir,3,14,"binary operand type mismatch"))return 31;

    memset(ir,0,sizeof(ir));
    put(ir,0,1,4,1,2,2,0,0);
    if(expect_verifier_error(ir,1,13,"use before definition"))return 32;

    memset(ir,0,sizeof(ir));
    put(ir,0,0,4,1,0,0,0,4);put(ir,1,23,4,2,1,0,0,0);
    if(expect_verifier_error(ir,2,14,"load requires pointer"))return 33;

    memset(ir,0,sizeof(ir));
    put(ir,0,0,4,1,0,0,0,1);put(ir,1,0,4,2,0,0,0,2);
    put(ir,2,0,4,3,0,0,0,3);put(ir,3,20,4,4,1,2,3,0);
    if(expect_verifier_error(ir,4,14,"select condition type"))return 34;

    memset(ir,0,sizeof(ir));
    put(ir,0,22,4,1,0,0,0,0);
    if(expect_verifier_error(ir,1,6,"alloca result type"))return 35;

    memset(ir,0,sizeof(ir));
    put(ir,0,25,4,1,0,1,0,0);
    if(expect_verifier_error(ir,1,16,"call argument layout"))return 36;

    memset(ir,0,sizeof(ir));
    put(ir,0,0,4,1,0,0,0,1);put(ir,1,0,4,2,0,0,0,2);
    put(ir,2,21,5,3,1,2,0,0);
    if(expect_verifier_error(ir,3,14,"phi incoming type"))return 37;
    return 0;
}
static int invalid_workspace(void){
    uint32_t ir[8]={0},ws[8]={0};
    int64_t status=swan_fn_swanc_ir_optimize(ir,1,ws,1);
    if(status==0||ws[0]!=13){
        fprintf(stderr,"undersized workspace: status=%lld diagnostic=%u\n",
                (long long)status,ws[0]);
        return 22;
    }
    memset(ws,0,sizeof(ws));
    status=swan_fn_swanc_ir_optimize(ir,1,ws,268435456);
    if(status==0||ws[0]!=13){
        fprintf(stderr,"oversized workspace capacity: status=%lld diagnostic=%u\n",
                (long long)status,ws[0]);
        return 23;
    }
    memset(ws,0,sizeof(ws));
    status=swan_fn_swanc_ir_optimize(ir,-1,ws,1);
    if(status==0||ws[0]!=13){
        fprintf(stderr,"negative instruction count: status=%lld diagnostic=%u\n",
                (long long)status,ws[0]);
        return 24;
    }
    return 0;
}
static int invalid_ir(void){
    uint32_t ir[8]={26,0,0,0,0,0,0,0},ws[8]={0};
    if(swan_fn_swanc_ir_optimize(ir,1,ws,2)==0)return 20;
    if(ws[0]!=2){fprintf(stderr,"invalid opcode diagnostic=%u\n",ws[0]);return 21;}
    return 0;
}
int main(void){
    int s=workload();if(s)return s;s=constant_fold_overflow();if(s)return s;s=edge_cases();if(s)return s;s=verifier_contracts();if(s)return s;s=invalid_workspace();if(s)return s;s=invalid_ir();if(s)return s;
    puts("Compiler 2 optimizer correctness passed: 128 rounds, 65,920 IR instructions, 16,384 folds; side effects, traps, calls, cascading DCE, SSA ordering, operand types, arity, and invalid IR checked.");
    return 0;
}
