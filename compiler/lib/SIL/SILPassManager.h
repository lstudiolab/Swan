#ifndef SWAN_SIL_PASS_MANAGER_H
#define SWAN_SIL_PASS_MANAGER_H
#include "SILGen.h"
#include "SILTransforms.h"
static inline bool swan_sil_optimize(SwanSILModule *m,SwanSILStats *stats,char *error,size_t cap) {
 static const char *names[]={"sil-canonicalization","sil-ownership-simplification","sil-dead-value-elimination"};
 (void)names;
 if(stats)memset(stats,0,sizeof(*stats));
 if(!swan_sil_verify(m,error,cap))return false;
 bool converged=false;size_t executed=0,rounds=0;
 while(executed<90&&swan_compiler_optimization_passes<SWAN_COMPILER_OPTIMIZATION_BUDGET){
  bool round_changed=false;rounds++;
  for(unsigned pass=0;pass<3&&executed<90&&swan_compiler_optimization_passes<SWAN_COMPILER_OPTIMIZATION_BUDGET;pass++,executed++){
   if(!swan_optimizer_consume(1)){if(error&&cap)snprintf(error,cap,"compiler-wide optimization budget exhausted during SIL");return false;}
   bool changed=false,ok=pass==0?swan_sil_canonicalize(m,&changed):
      pass==1?swan_sil_ownership_simplify(m,&changed):swan_sil_dead_value_elimination(m,&changed);
   if(!ok){if(error&&cap)snprintf(error,cap,"SIL optimization pass failed: %s",names[pass]);return false;}
   round_changed|=changed;
  }
  if(!swan_sil_verify(m,error,cap))return false;
  if(!round_changed){converged=true;break;}
 }
 if(stats){stats->passes_executed=executed;stats->rounds=rounds;stats->converged=converged;}
 if(!converged){if(error&&cap)snprintf(error,cap,"SIL optimization did not converge within its 90-pass stage budget or compiler-wide 350-pass budget");return false;}
 return swan_sil_verify(m,error,cap);
}
static inline bool swan_sil_prepare_program(const Program *program,SwanError *error) {
 SwanSILModule module={0};SwanSILStats stats={0};char detail[256]={0};
 bool ok=swan_silgen_program(program,&module,error);
 if(ok)ok=swan_sil_optimize(&module,&stats,detail,sizeof(detail));
 if(!ok&&error&&!error->message[0])swan_error(error,NULL,"SIL pipeline failed: %s",detail);
 swan_sil_destroy(&module);return ok;
}

#endif
