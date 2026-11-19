#include "brew/optimization.h"
#include <assert.h>
#include <stdio.h>
static void check_or_all_ones(BrewIRType type,bool constant_left){
 BrewIRModule module={0};
 BrewIRFunction *fn=brew_ir_add_function(&module,"or_all_ones",type,&type,1);assert(fn);
 BrewIRBlock *entry=brew_ir_add_block(fn,"entry");assert(entry);
 BrewIRInst constant={0};constant.op=BREW_IR_CONST;constant.type=type;constant.immediate=-1;
 uint32_t all=brew_ir_add_inst(fn,entry,constant);assert(all);
 BrewIRInst op={0};op.op=BREW_IR_OR;op.type=type;
 op.a=constant_left?all:1;op.b=constant_left?1:all;
 uint32_t result=brew_ir_add_inst(fn,entry,op);assert(result);
 assert(brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,result,0,0}));
 char error[256]={0};assert(brew_ir_optimize(&module,error,sizeof(error)));
 assert(brew_ir_verify(&module,error,sizeof(error)));
 bool found=false;
 for(size_t i=0;i<entry->instruction_count;i++)if(entry->instructions[i].result==result){
  assert(entry->instructions[i].op==BREW_IR_CONST);
  assert(entry->instructions[i].immediate==-1);found=true;
 }
 assert(found);brew_ir_destroy(&module);
}
static void check_unreachable_block_cleanup(void){
 BrewIRModule module={0};BrewIRType params[]={BREW_IR_I64};
 BrewIRFunction *callee=brew_ir_add_function(&module,"dead_helper",BREW_IR_I64,params,1);assert(callee);
 callee->external=true;
 BrewIRFunction *fn=brew_ir_add_function(&module,"dead_cleanup",BREW_IR_I64,params,1);assert(fn);
 BrewIRBlock *entry=brew_ir_add_block(fn,"entry"),*live=brew_ir_add_block(fn,"live");
 BrewIRBlock *dead=brew_ir_add_block(fn,"dead"),*dead_tail=brew_ir_add_block(fn,"dead_tail");
 assert(entry&&live&&dead&&dead_tail);
 assert(brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_BR,0,1,0}));
 assert(brew_ir_terminate(live,(BrewIRTerm){BREW_IR_RET,1,0,0}));
 BrewIRInst call={0};call.op=BREW_IR_CALL;call.type=BREW_IR_I64;call.symbol="dead_helper";
 call.incoming=calloc(1,sizeof(*call.incoming));assert(call.incoming);call.incoming_count=1;
 call.incoming[0]=(BrewIRIncoming){1,0};
 assert(brew_ir_add_inst(fn,dead,call));
 assert(brew_ir_terminate(dead,(BrewIRTerm){BREW_IR_BR,0,3,0}));
 assert(brew_ir_terminate(dead_tail,(BrewIRTerm){BREW_IR_RET,1,0,0}));
 char error[256]={0};assert(brew_ir_verify(&module,error,sizeof(error)));
 assert(brew_ir_optimize(&module,error,sizeof(error)));
 assert(brew_ir_verify(&module,error,sizeof(error)));
 assert(fn->block_count==2&&fn->blocks[0].terminator.target==1);
 brew_ir_destroy(&module);
}
static void check_constant_chain(size_t length){
 BrewIRModule module={0};
 BrewIRFunction *fn=brew_ir_add_function(&module,"constant_chain",BREW_IR_I64,NULL,0);assert(fn);
 BrewIRBlock *entry=brew_ir_add_block(fn,"entry");assert(entry);
 BrewIRInst one={0};one.op=BREW_IR_CONST;one.type=BREW_IR_I64;one.immediate=1;
 uint32_t one_id=brew_ir_add_inst(fn,entry,one);assert(one_id);
 uint32_t value=one_id;
 for(size_t i=0;i<length;i++){
  BrewIRInst add={0};add.op=BREW_IR_ADD;add.type=BREW_IR_I64;add.a=value;add.b=one_id;
  value=brew_ir_add_inst(fn,entry,add);assert(value);
 }
 assert(brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,value,0,0}));
 char error[256]={0};assert(brew_ir_optimize(&module,error,sizeof(error)));
 assert(brew_ir_verify(&module,error,sizeof(error)));
 assert(entry->instruction_count==1);
 assert(entry->instructions[0].op==BREW_IR_CONST);
 assert(entry->instructions[0].immediate==(int64_t)(length+1));
 brew_ir_destroy(&module);
}
static void check_optimizer_budget(void){
 BrewIRModule module={0};
 BrewIRFunction *fn=brew_ir_add_function(&module,"budget",BREW_IR_I64,NULL,0);assert(fn);
 BrewIRBlock *entry=brew_ir_add_block(fn,"entry");assert(entry);
 BrewIRInst one={0};one.op=BREW_IR_CONST;one.type=BREW_IR_I64;one.immediate=20;
 uint32_t a=brew_ir_add_inst(fn,entry,one);assert(a);
 BrewIRInst two=one;two.immediate=22;
 uint32_t b=brew_ir_add_inst(fn,entry,two);assert(b);
 BrewIRInst add={0};add.op=BREW_IR_ADD;add.type=BREW_IR_I64;add.a=a;add.b=b;
 uint32_t result=brew_ir_add_inst(fn,entry,add);assert(result);
 assert(brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,result,0,0}));
 BrewIROptStats stats={0};char error[256]={0};
 assert(brew_ir_optimize_with_stats(&module,&stats,error,sizeof(error)));
 assert(stats.converged&&stats.rounds>0);
 assert(stats.passes_executed>0&&stats.passes_executed<=BREW_IR_MAX_OPTIMIZATION_PASSES);
 assert(stats.passes_executed==stats.rounds*
        (sizeof(brew_ir_default_passes)/sizeof(brew_ir_default_passes[0])));
 assert(brew_ir_verify(&module,error,sizeof(error)));
 brew_ir_destroy(&module);
}
static void check_transitive_copy_propagation(void){
 BrewIRModule module={0};BrewIRType params[]={BREW_IR_I1,BREW_IR_I64};
 BrewIRFunction *fn=brew_ir_add_function(&module,"copy_chain",BREW_IR_I64,params,2);assert(fn);
 BrewIRBlock *entry=brew_ir_add_block(fn,"entry");assert(entry);
 BrewIRInst first={0};first.op=BREW_IR_SELECT;first.type=BREW_IR_I64;first.a=1;first.b=2;first.c=2;
 uint32_t first_copy=brew_ir_add_inst(fn,entry,first);assert(first_copy==3);
 BrewIRInst second=first;second.b=first_copy;second.c=first_copy;
 uint32_t second_copy=brew_ir_add_inst(fn,entry,second);assert(second_copy==4);
 BrewIRInst add={0};add.op=BREW_IR_ADD;add.type=BREW_IR_I64;add.a=second_copy;add.b=first_copy;
 uint32_t result=brew_ir_add_inst(fn,entry,add);assert(result==5);
 assert(brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,result,0,0}));
 char error[256]={0};assert(brew_ir_verify(&module,error,sizeof(error)));
 bool changed=false;
 assert(brew_ir_copy_propagation(&module,&changed,error,sizeof(error))&&changed);
 assert(brew_ir_dead_code_eliminate(&module,&changed));
 assert(brew_ir_verify(&module,error,sizeof(error)));
 assert(entry->instruction_count==1);
 assert(entry->instructions[0].op==BREW_IR_ADD);
 assert(entry->instructions[0].a==2&&entry->instructions[0].b==2);
 brew_ir_destroy(&module);
}
static void check_trapping_operand_is_preserved(void){
 BrewIRModule module={0};
 BrewIRFunction *fn=brew_ir_add_function(&module,"preserve_trap",BREW_IR_I64,NULL,0);assert(fn);
 BrewIRBlock *entry=brew_ir_add_block(fn,"entry");assert(entry);
 BrewIRInst one={0};one.op=BREW_IR_CONST;one.type=BREW_IR_I64;one.immediate=1;
 uint32_t one_id=brew_ir_add_inst(fn,entry,one);assert(one_id);
 BrewIRInst zero={0};zero.op=BREW_IR_CONST;zero.type=BREW_IR_I64;zero.immediate=0;
 uint32_t zero_id=brew_ir_add_inst(fn,entry,zero);assert(zero_id);
 BrewIRInst divide={0};divide.op=BREW_IR_SDIV;divide.type=BREW_IR_I64;divide.a=one_id;divide.b=zero_id;
 uint32_t quotient=brew_ir_add_inst(fn,entry,divide);assert(quotient);
 BrewIRInst multiply={0};multiply.op=BREW_IR_MUL;multiply.type=BREW_IR_I64;multiply.a=quotient;multiply.b=zero_id;
 uint32_t result=brew_ir_add_inst(fn,entry,multiply);assert(result);
 assert(brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_RET,result,0,0}));
 char error[256]={0};assert(brew_ir_optimize(&module,error,sizeof(error)));
 assert(brew_ir_verify(&module,error,sizeof(error)));
 bool division_survived=false,multiply_survived=false;
 for(size_t i=0;i<entry->instruction_count;i++){
  division_survived|=entry->instructions[i].op==BREW_IR_SDIV;
  multiply_survived|=entry->instructions[i].result==result&&entry->instructions[i].op==BREW_IR_MUL;
 }
 assert(division_survived&&multiply_survived);
 brew_ir_destroy(&module);
}
int main(void){
 BrewIRModule m={0};BrewIRFunction *f=brew_ir_add_function(&m,"main",BREW_IR_I64,NULL,0);assert(f);
 BrewIRBlock *b=brew_ir_add_block(f,"entry");assert(b);
 BrewIRInst a={0};a.op=BREW_IR_CONST;a.type=BREW_IR_I64;a.immediate=20;uint32_t av=brew_ir_add_inst(f,b,a);
 BrewIRInst x={0};x.op=BREW_IR_CONST;x.type=BREW_IR_I64;x.immediate=22;uint32_t xv=brew_ir_add_inst(f,b,x);
 BrewIRInst unused={0};unused.op=BREW_IR_CONST;unused.type=BREW_IR_I64;unused.immediate=999;assert(brew_ir_add_inst(f,b,unused));
 BrewIRInst sum={0};sum.op=BREW_IR_ADD;sum.type=BREW_IR_I64;sum.a=av;sum.b=xv;uint32_t result=brew_ir_add_inst(f,b,sum);
 assert(brew_ir_terminate(b,(BrewIRTerm){BREW_IR_RET,result,0,0}));
 char error[256]={0};assert(brew_ir_optimize(&m,error,sizeof(error)));
 assert(b->instruction_count==1);
 assert(b->instructions[0].op==BREW_IR_CONST&&b->instructions[0].immediate==42);
 assert(brew_ir_verify(&m,error,sizeof(error)));brew_ir_destroy(&m);
 BrewIRModule cse={0};BrewIRType cparams[]={BREW_IR_I64,BREW_IR_I64};BrewIRFunction *cf=brew_ir_add_function(&cse,"cse",BREW_IR_I64,cparams,2);assert(cf);
 BrewIRBlock *cb=brew_ir_add_block(cf,"entry");assert(cb);
 BrewIRInst add1={0};add1.op=BREW_IR_ADD;add1.type=BREW_IR_I64;add1.a=1;add1.b=2;uint32_t sum1=brew_ir_add_inst(cf,cb,add1);
 BrewIRInst add2=add1;add2.a=2;add2.b=1;uint32_t sum2=brew_ir_add_inst(cf,cb,add2);
 BrewIRInst mul={0};mul.op=BREW_IR_MUL;mul.type=BREW_IR_I64;mul.a=sum2;mul.b=1;uint32_t product=brew_ir_add_inst(cf,cb,mul);
 assert(brew_ir_terminate(cb,(BrewIRTerm){BREW_IR_RET,product,0,0}));
 assert(brew_ir_optimize(&cse,error,sizeof(error)));assert(brew_ir_verify(&cse,error,sizeof(error)));assert(cb->instruction_count==2);assert(cb->instructions[0].op==BREW_IR_ADD);assert(cb->instructions[1].op==BREW_IR_MUL&&cb->instructions[1].a==sum1);
 brew_ir_destroy(&cse);
 BrewIRModule cfg={0};BrewIRFunction *gf=brew_ir_add_function(&cfg,"main",BREW_IR_I64,NULL,0);assert(gf);
 BrewIRBlock *entry=brew_ir_add_block(gf,"entry"),*yes=brew_ir_add_block(gf,"yes"),*no=brew_ir_add_block(gf,"no");assert(entry&&yes&&no);
 BrewIRInst condition={0};condition.op=BREW_IR_CONST;condition.type=BREW_IR_I1;condition.immediate=0;uint32_t cv=brew_ir_add_inst(gf,entry,condition);
 BrewIRInst one={0};one.op=BREW_IR_CONST;one.type=BREW_IR_I64;one.immediate=1;uint32_t ov=brew_ir_add_inst(gf,yes,one);
 BrewIRInst two={0};two.op=BREW_IR_CONST;two.type=BREW_IR_I64;two.immediate=2;uint32_t tv=brew_ir_add_inst(gf,no,two);
 assert(brew_ir_terminate(entry,(BrewIRTerm){BREW_IR_COND_BR,cv,1,2}));
 assert(brew_ir_terminate(yes,(BrewIRTerm){BREW_IR_RET,ov,0,0}));
 assert(brew_ir_terminate(no,(BrewIRTerm){BREW_IR_RET,tv,0,0}));
 assert(brew_ir_optimize(&cfg,error,sizeof(error)));assert(gf->block_count==2);
 brew_ir_destroy(&cfg);
 BrewIRModule global={0};BrewIRType gp[]={BREW_IR_I64,BREW_IR_I64};
 BrewIRFunction *g=brew_ir_add_function(&global,"global_cse",BREW_IR_I64,gp,2);assert(g);
 BrewIRBlock *ge=brew_ir_add_block(g,"entry"),*gb=brew_ir_add_block(g,"dominated");assert(ge&&gb);
 BrewIRInst ga={0};ga.op=BREW_IR_ADD;ga.type=BREW_IR_I64;ga.a=1;ga.b=2;uint32_t first=brew_ir_add_inst(g,ge,ga);assert(first);
 assert(brew_ir_terminate(ge,(BrewIRTerm){BREW_IR_BR,0,1,0}));
 uint32_t duplicate=brew_ir_add_inst(g,gb,ga);assert(duplicate);
 assert(brew_ir_terminate(gb,(BrewIRTerm){BREW_IR_RET,duplicate,0,0}));
 assert(brew_ir_optimize(&global,error,sizeof(error)));assert(brew_ir_verify(&global,error,sizeof(error)));
 assert(g->blocks[0].instruction_count==1&&g->blocks[1].instruction_count==0);
 brew_ir_destroy(&global);
 BrewIRModule siblings={0};BrewIRType sp[]={BREW_IR_I64,BREW_IR_I64,BREW_IR_I1};
 BrewIRFunction *sf=brew_ir_add_function(&siblings,"sibling_cse",BREW_IR_I64,sp,3);assert(sf);
 BrewIRBlock *se=brew_ir_add_block(sf,"entry"),*sy=brew_ir_add_block(sf,"yes"),*sn=brew_ir_add_block(sf,"no");assert(se&&sy&&sn);
 assert(brew_ir_terminate(se,(BrewIRTerm){BREW_IR_COND_BR,3,1,2}));
 BrewIRInst sa={0};sa.op=BREW_IR_ADD;sa.type=BREW_IR_I64;sa.a=1;sa.b=2;
 uint32_t syv=brew_ir_add_inst(sf,sy,sa),snv=brew_ir_add_inst(sf,sn,sa);assert(syv&&snv);
 assert(brew_ir_terminate(sy,(BrewIRTerm){BREW_IR_RET,syv,0,0}));
 assert(brew_ir_terminate(sn,(BrewIRTerm){BREW_IR_RET,snv,0,0}));
 assert(brew_ir_optimize(&siblings,error,sizeof(error)));assert(brew_ir_verify(&siblings,error,sizeof(error)));
 assert(sf->blocks[1].instruction_count==1&&sf->blocks[2].instruction_count==1);
 brew_ir_destroy(&siblings);
 BrewIRModule copies={0};BrewIRType cp[]={BREW_IR_I64,BREW_IR_I1};
 BrewIRFunction *copy_fn=brew_ir_add_function(&copies,"copy",BREW_IR_I64,cp,2);assert(copy_fn);
 BrewIRBlock *copy_entry=brew_ir_add_block(copy_fn,"entry"),*copy_yes=brew_ir_add_block(copy_fn,"yes");
 BrewIRBlock *copy_no=brew_ir_add_block(copy_fn,"no"),*copy_merge=brew_ir_add_block(copy_fn,"merge");
 assert(copy_entry&&copy_yes&&copy_no&&copy_merge);
 assert(brew_ir_terminate(copy_entry,(BrewIRTerm){BREW_IR_COND_BR,2,1,2}));
 assert(brew_ir_terminate(copy_yes,(BrewIRTerm){BREW_IR_BR,0,3,0}));
 assert(brew_ir_terminate(copy_no,(BrewIRTerm){BREW_IR_BR,0,3,0}));
 BrewIRInst trivial_phi={0};trivial_phi.op=BREW_IR_PHI;trivial_phi.type=BREW_IR_I64;
 trivial_phi.incoming=calloc(2,sizeof(*trivial_phi.incoming));assert(trivial_phi.incoming);trivial_phi.incoming_count=2;
 trivial_phi.incoming[0]=(BrewIRIncoming){1,1};trivial_phi.incoming[1]=(BrewIRIncoming){1,2};
 uint32_t copied=brew_ir_add_inst(copy_fn,copy_merge,trivial_phi);assert(copied);
 assert(brew_ir_terminate(copy_merge,(BrewIRTerm){BREW_IR_RET,copied,0,0}));
 assert(brew_ir_optimize(&copies,error,sizeof(error)));assert(brew_ir_verify(&copies,error,sizeof(error)));
 assert(copy_merge->instruction_count==0&&copy_merge->terminator.value==1);
 brew_ir_destroy(&copies);
  BrewIRModule licm={0};BrewIRType lp[]={BREW_IR_I64};
 BrewIRFunction *lf=brew_ir_add_function(&licm,"work",BREW_IR_I64,lp,1);assert(lf);
 BrewIRBlock *le=brew_ir_add_block(lf,"entry"),*lh=brew_ir_add_block(lf,"header");
 BrewIRBlock *lb=brew_ir_add_block(lf,"body"),*lx=brew_ir_add_block(lf,"exit");assert(le&&lh&&lb&&lx);
 BrewIRInst lzero={0};lzero.op=BREW_IR_CONST;lzero.type=BREW_IR_I64;lzero.immediate=0;
 uint32_t lz=brew_ir_add_inst(lf,le,lzero);assert(lz);
 BrewIRInst llimit={0};llimit.op=BREW_IR_CONST;llimit.type=BREW_IR_I64;llimit.immediate=5;
 uint32_t lim=brew_ir_add_inst(lf,le,llimit);assert(lim);
 BrewIRInst lthree={0};lthree.op=BREW_IR_CONST;lthree.type=BREW_IR_I64;lthree.immediate=3;
 uint32_t three=brew_ir_add_inst(lf,le,lthree);assert(three);
 assert(brew_ir_terminate(le,(BrewIRTerm){BREW_IR_BR,0,1,0}));
 BrewIRInst ip={0};ip.op=BREW_IR_PHI;ip.type=BREW_IR_I64;ip.incoming=calloc(2,sizeof(*ip.incoming));assert(ip.incoming);ip.incoming_count=2;ip.incoming[0]=(BrewIRIncoming){lz,0};
 uint32_t iv=brew_ir_add_inst(lf,lh,ip);assert(iv);
 BrewIRInst sum_phi={0};sum_phi.op=BREW_IR_PHI;sum_phi.type=BREW_IR_I64;sum_phi.incoming=calloc(2,sizeof(*sum_phi.incoming));assert(sum_phi.incoming);sum_phi.incoming_count=2;sum_phi.incoming[0]=(BrewIRIncoming){lz,0};
 uint32_t sv=brew_ir_add_inst(lf,lh,sum_phi);assert(sv);
 BrewIRInst cmp={0};cmp.op=BREW_IR_ICMP_SLT;cmp.type=BREW_IR_I1;cmp.a=iv;cmp.b=lim;
 uint32_t loop_condition=brew_ir_add_inst(lf,lh,cmp);assert(loop_condition);
 assert(brew_ir_terminate(lh,(BrewIRTerm){BREW_IR_COND_BR,loop_condition,2,3}));
 BrewIRInst invariant={0};invariant.op=BREW_IR_MUL;invariant.type=BREW_IR_I64;invariant.a=1;invariant.b=three;
 uint32_t loop_product=brew_ir_add_inst(lf,lb,invariant);assert(loop_product);
 BrewIRInst invariant_sum={0};invariant_sum.op=BREW_IR_ADD;invariant_sum.type=BREW_IR_I64;invariant_sum.a=loop_product;invariant_sum.b=three;
 uint32_t invariant_sum_id=brew_ir_add_inst(lf,lb,invariant_sum);assert(invariant_sum_id);
 BrewIRInst accum={0};accum.op=BREW_IR_ADD;accum.type=BREW_IR_I64;accum.a=sv;accum.b=invariant_sum_id;
 uint32_t accumulated=brew_ir_add_inst(lf,lb,accum);assert(accumulated);
 BrewIRInst lone={0};lone.op=BREW_IR_CONST;lone.type=BREW_IR_I64;lone.immediate=1;
 uint32_t increment=brew_ir_add_inst(lf,lb,lone);assert(increment);
 BrewIRInst step={0};step.op=BREW_IR_ADD;step.type=BREW_IR_I64;step.a=iv;step.b=increment;
 uint32_t ivnext=brew_ir_add_inst(lf,lb,step);assert(ivnext);
 ip.incoming[1]=(BrewIRIncoming){ivnext,2};sum_phi.incoming[1]=(BrewIRIncoming){accumulated,2};
 assert(brew_ir_terminate(lb,(BrewIRTerm){BREW_IR_BR,0,1,0}));
 assert(brew_ir_terminate(lx,(BrewIRTerm){BREW_IR_RET,sv,0,0}));
 BrewIRFunction *lm=brew_ir_add_function(&licm,"main",BREW_IR_I64,NULL,0);assert(lm);
 BrewIRBlock *lme=brew_ir_add_block(lm,"entry");assert(lme);
 BrewIRInst arg={0};arg.op=BREW_IR_CONST;arg.type=BREW_IR_I64;arg.immediate=3;
 uint32_t arg_id=brew_ir_add_inst(lm,lme,arg);assert(arg_id);
 BrewIRInst invoke={0};invoke.op=BREW_IR_CALL;invoke.type=BREW_IR_I64;invoke.symbol="work";
 invoke.incoming=calloc(1,sizeof(*invoke.incoming));assert(invoke.incoming);invoke.incoming_count=1;invoke.incoming[0]=(BrewIRIncoming){arg_id,0};
 uint32_t invoked=brew_ir_add_inst(lm,lme,invoke);assert(invoked);
 assert(brew_ir_terminate(lme,(BrewIRTerm){BREW_IR_RET,invoked,0,0}));
 assert(brew_ir_optimize(&licm,error,sizeof(error)));assert(brew_ir_verify(&licm,error,sizeof(error)));
 bool hoisted_product=false,hoisted_sum=false,remains_product=false,remains_sum=false;
 for(size_t i=0;i<lf->blocks[0].instruction_count;i++){
  hoisted_product|=lf->blocks[0].instructions[i].result==loop_product;
  hoisted_sum|=lf->blocks[0].instructions[i].result==invariant_sum_id;
 }
 for(size_t i=0;i<lf->blocks[2].instruction_count;i++){
  remains_product|=lf->blocks[2].instructions[i].result==loop_product;
  remains_sum|=lf->blocks[2].instructions[i].result==invariant_sum_id;
 }
 assert(hoisted_product&&hoisted_sum&&!remains_product&&!remains_sum);
 brew_ir_destroy(&licm);

 check_optimizer_budget();
 check_trapping_operand_is_preserved();
 check_unreachable_block_cleanup();
 check_transitive_copy_propagation();
 check_constant_chain(512);
 check_or_all_ones(BREW_IR_I8,false);check_or_all_ones(BREW_IR_I8,true);
 check_or_all_ones(BREW_IR_I16,false);check_or_all_ones(BREW_IR_I16,true);
 check_or_all_ones(BREW_IR_I32,false);check_or_all_ones(BREW_IR_I32,true);
 check_or_all_ones(BREW_IR_I64,false);check_or_all_ones(BREW_IR_I64,true);
 puts("PASS: bounded fixed-point optimization, pass accounting, constant folding, CFG simplification, algebraic identities, copy propagation, dominance-aware CSE, LICM, DCE, and all-width OR identities");
 return 0;
}
