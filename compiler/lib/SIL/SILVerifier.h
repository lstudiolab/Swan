#ifndef BREW_SIL_VERIFIER_H
#define BREW_SIL_VERIFIER_H
#include "SILCore.h"
static inline bool brew_sil_verify(const BrewSILModule *m,char *error,size_t cap) {
 if(!m){if(error&&cap)snprintf(error,cap,"null SIL module");return false;}
 for(size_t fi=0;fi<m->function_count;fi++){
  const BrewSILFunction *f=&m->functions[fi];
  if(!f->block_count||!f->blocks){if(error&&cap)snprintf(error,cap,"SIL function %s has no entry basic block",f->name);return false;}
  for(size_t bi=0;bi<f->block_count;bi++){
   const BrewSILBasicBlock *block=&f->blocks[bi];
   if(block->id!=bi||block->successor_count>2){if(error&&cap)snprintf(error,cap,"malformed SIL basic block in %s",f->name);return false;}
   for(size_t edge=0;edge<block->successor_count;edge++)if(block->successors[edge]>=f->block_count){if(error&&cap)snprintf(error,cap,"SIL branch target out of range in %s",f->name);return false;}
   if(block->instruction_count){
    if(block->first_instruction==SIZE_MAX||block->first_instruction>f->instruction_count||
       block->instruction_count>f->instruction_count-block->first_instruction){if(error&&cap)snprintf(error,cap,"invalid SIL instruction range in %s",f->name);return false;}
    for(size_t k=0;k<block->instruction_count;k++){
     const BrewSILInstruction *item=&f->instructions[block->first_instruction+k];
     if(item->block!=bi||(item->terminates_block!=(k+1==block->instruction_count&&block->terminated))){if(error&&cap)snprintf(error,cap,"invalid SIL block termination in %s",f->name);return false;}
    }
   }else if(block->terminated){if(error&&cap)snprintf(error,cap,"empty SIL block is marked terminated in %s",f->name);return false;}
  }
  for(size_t i=0;i<f->instruction_count;i++){
   const BrewSILInstruction *in=&f->instructions[i];
   if(in->id!=i+1||in->block>=f->block_count||in->operand_count>3||in->target_count>2){if(error&&cap)snprintf(error,cap,"malformed SIL instruction in %s at %zu",f->name,i);return false;}
   for(size_t k=0;k<in->operand_count;k++)if(!in->operands[k]||in->operands[k]>=in->id){
    if(error&&cap){snprintf(error,cap,"SIL use-before-definition in %s at instruction %zu",f->name,i+1);}
    return false;
   }
   if(in->ownership==BREW_SIL_GUARANTEED&&in->type!=BREW_SIL_TYPE_OBJECT){
    if(error&&cap){snprintf(error,cap,"guaranteed ownership applied to non-object SIL value in %s",f->name);}
    return false;
   }
   if((in->opcode==BREW_SIL_BRANCH&&in->target_count!=1)||
      (in->opcode==BREW_SIL_COND_BRANCH&&in->target_count!=2)){
    if(error&&cap)snprintf(error,cap,"SIL branch has incorrect successor count in %s",f->name);
    return false;
   }
   if((in->terminates_block&&in->target_count!=f->blocks[in->block].successor_count)||
      (!in->terminates_block&&in->target_count!=0)){
    if(error&&cap)snprintf(error,cap,"SIL branch successor metadata mismatch in %s",f->name);
    return false;
   }
   if(in->opcode==BREW_SIL_STORE&&in->operand_count!=2){
    if(error&&cap){snprintf(error,cap,"SIL store requires address and value in %s",f->name);}
    return false;
   }
  }
 }
 return true;
}

#endif
