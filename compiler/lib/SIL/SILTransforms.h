#ifndef BREW_SIL_TRANSFORMS_H
#define BREW_SIL_TRANSFORMS_H
#include "SILVerifier.h"
static inline bool brew_sil_canonicalize(BrewSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++)for(size_t i=0;i<m->functions[f].instruction_count;i++){
  BrewSILInstruction *in=&m->functions[f].instructions[i];
  if(in->opcode==BREW_SIL_COPY_VALUE&&in->operand_count==1){in->deleted=true;any=true;}
  if(in->opcode==BREW_SIL_BORROW_VALUE&&in->ownership!=BREW_SIL_GUARANTEED){in->ownership=BREW_SIL_GUARANTEED;any=true;}
 }
 if(changed)*changed=any;
 return true;
}
static inline bool brew_sil_dead_value_elimination(BrewSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++){
  BrewSILFunction *fn=&m->functions[f];bool again=true;
  while(again){again=false;
   for(size_t i=0;i<fn->instruction_count;i++){
    BrewSILInstruction *in=&fn->instructions[i];if(in->deleted||in->side_effecting||in->opcode==BREW_SIL_RETURN||in->opcode==BREW_SIL_BRANCH||in->opcode==BREW_SIL_COND_BRANCH)continue;
    bool used=false;for(size_t j=0;j<fn->instruction_count&&!used;j++)if(!fn->instructions[j].deleted)
     for(size_t k=0;k<fn->instructions[j].operand_count;k++)if(fn->instructions[j].operands[k]==in->id){used=true;break;}
    if(!used){in->deleted=true;again=any=true;}
   }
  }
 }
 if(changed)*changed=any;
 return true;
}
static inline bool brew_sil_ownership_simplify(BrewSILModule *m,bool *changed) {
 bool any=false;
 for(size_t f=0;f<m->function_count;f++)for(size_t i=0;i<m->functions[f].instruction_count;i++){
  BrewSILInstruction *in=&m->functions[f].instructions[i];
  if(in->type==BREW_SIL_TYPE_TRIVIAL&&in->ownership!=BREW_SIL_OWNERSHIP_NONE){in->ownership=BREW_SIL_OWNERSHIP_NONE;any=true;}
 }
 if(changed)*changed=any;
 return true;
}

#endif
