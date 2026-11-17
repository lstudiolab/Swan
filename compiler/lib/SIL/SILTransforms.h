#ifndef SWAN_SIL_TRANSFORMS_H
#define SWAN_SIL_TRANSFORMS_H
#include "SILVerifier.h"
static inline bool swan_sil_canonicalize(SwanSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++)for(size_t i=0;i<m->functions[f].instruction_count;i++){
  SwanSILInstruction *in=&m->functions[f].instructions[i];
  if(in->opcode==SWAN_SIL_COPY_VALUE&&in->operand_count==1){in->deleted=true;any=true;}
  if(in->opcode==SWAN_SIL_BORROW_VALUE&&in->ownership!=SWAN_SIL_GUARANTEED){in->ownership=SWAN_SIL_GUARANTEED;any=true;}
 }
 if(changed)*changed=any;
 return true;
}
static inline bool swan_sil_dead_value_elimination(SwanSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++){
  SwanSILFunction *fn=&m->functions[f];bool again=true;
  while(again){again=false;
   for(size_t i=0;i<fn->instruction_count;i++){
    SwanSILInstruction *in=&fn->instructions[i];if(in->deleted||in->side_effecting||in->opcode==SWAN_SIL_RETURN||in->opcode==SWAN_SIL_BRANCH||in->opcode==SWAN_SIL_COND_BRANCH)continue;
    bool used=false;for(size_t j=0;j<fn->instruction_count&&!used;j++)if(!fn->instructions[j].deleted)
     for(size_t k=0;k<fn->instructions[j].operand_count;k++)if(fn->instructions[j].operands[k]==in->id){used=true;break;}
    if(!used){in->deleted=true;again=any=true;}
   }
  }
 }
 if(changed)*changed=any;
 return true;
}
static inline bool swan_sil_ownership_simplify(SwanSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++)for(size_t i=0;i<m->functions[f].instruction_count;i++){
  SwanSILInstruction *in=&m->functions[f].instructions[i];
  if(in->type==SWAN_SIL_TYPE_TRIVIAL&&in->ownership!=SWAN_SIL_OWNERSHIP_NONE){in->ownership=SWAN_SIL_OWNERSHIP_NONE;any=true;}
 }
 if(changed)*changed=any;return true;
}

#endif
