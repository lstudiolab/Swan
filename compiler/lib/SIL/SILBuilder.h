#ifndef SWAN_SIL_BUILDER_H
#define SWAN_SIL_BUILDER_H
#include "SILCore.h"
static inline uint32_t swan_sil_add_block(SwanSILFunction *f) {
 if(!f||f->block_count>=UINT32_MAX||!swan_sil_reserve((void**)&f->blocks,&f->block_capacity,f->block_count+1,sizeof(*f->blocks)))return UINT32_MAX;
 uint32_t id=(uint32_t)f->block_count;SwanSILBasicBlock *b=&f->blocks[f->block_count++];memset(b,0,sizeof(*b));b->id=id;b->first_instruction=SIZE_MAX;return id;
}
static inline SwanSILFunction *swan_sil_add_function(SwanSILModule *m,const char *name,size_t params,bool external) {
 if(!m||!name||!swan_sil_reserve((void**)&m->functions,&m->function_capacity,m->function_count+1,sizeof(*m->functions)))return NULL;
 SwanSILFunction *f=&m->functions[m->function_count++];memset(f,0,sizeof(*f));
 f->name=name;f->parameter_count=params;f->external=external;
 if(swan_sil_add_block(f)==UINT32_MAX){m->function_count--;return NULL;}
 return f;
}
static inline uint32_t swan_sil_emit(SwanSILModule *m,SwanSILFunction *f,SwanSILInstruction in) {
 if(!m||!f||f->instruction_count>=UINT32_MAX||
    !swan_sil_reserve((void**)&f->instructions,&f->instruction_capacity,f->instruction_count+1,sizeof(*f->instructions)))return 0;
 in.id=(uint32_t)f->instruction_count+1;in.block=(uint32_t)f->current_block;
 if(f->current_block>=f->block_count)return 0;
 SwanSILBasicBlock *block=&f->blocks[f->current_block];
 if(block->terminated||in.target_count>2)return 0;
 if(block->first_instruction==SIZE_MAX)block->first_instruction=f->instruction_count;
 block->instruction_count++;
 if(in.terminates_block){block->terminated=true;block->successor_count=in.target_count;for(size_t i=0;i<in.target_count;i++)block->successors[i]=in.targets[i];}
 f->instructions[f->instruction_count++]=in;m->instruction_count++;return in.id;
}
static inline bool swan_sil_branch(SwanSILModule *m,SwanSILFunction *f,uint32_t target,const Token *location) {
 SwanSILInstruction branch={0};branch.opcode=SWAN_SIL_BRANCH;branch.type=SWAN_SIL_TYPE_TRIVIAL;branch.targets[0]=target;branch.target_count=1;branch.terminates_block=true;branch.side_effecting=true;branch.location=location;return swan_sil_emit(m,f,branch)!=0;
}
static inline bool swan_sil_cond_branch(SwanSILModule *m,SwanSILFunction *f,uint32_t condition,uint32_t yes,uint32_t no,const Token *location) {
 SwanSILInstruction branch={0};branch.opcode=SWAN_SIL_COND_BRANCH;branch.type=SWAN_SIL_TYPE_TRIVIAL;branch.operands[0]=condition;branch.operand_count=1;branch.targets[0]=yes;branch.targets[1]=no;branch.target_count=2;branch.terminates_block=true;branch.side_effecting=true;branch.location=location;return swan_sil_emit(m,f,branch)!=0;
}

#endif
