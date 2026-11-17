#ifndef SWAN_SIL_CORE_H
#define SWAN_SIL_CORE_H
#include "SILInstruction.h"
#include "../AST/AST.h"
typedef struct {
 uint32_t id, block;
 uint32_t targets[2];
 size_t target_count;
 SwanSILOpcode opcode;
 SwanSILTypeKind type;
 SwanSILOwnership ownership;
 uint32_t operands[3];
 size_t operand_count;
 int64_t integer;
 const char *text;
 const Token *location;
 bool side_effecting;
 bool terminates_block;
 bool deleted;
} SwanSILInstruction;
typedef struct {
 uint32_t id;
 uint32_t successors[2];
 size_t successor_count;
 size_t first_instruction, instruction_count;
 bool terminated;
} SwanSILBasicBlock;
typedef struct { uint32_t break_block, continue_block; } SwanSILLoopTarget;
typedef struct {
 const char *name;
 SwanSILInstruction *instructions;
 size_t instruction_count, instruction_capacity;
 size_t parameter_count;
 SwanSILBasicBlock *blocks;
 size_t block_count, block_capacity, current_block;
 SwanSILLoopTarget *loops;
 size_t loop_count, loop_capacity;
 bool external;
} SwanSILFunction;
typedef struct {
 SwanSILFunction *functions;
 size_t function_count, function_capacity;
 size_t instruction_count;
} SwanSILModule;
typedef struct {
 size_t passes_executed, rounds;
 bool converged;
} SwanSILStats;

static inline bool swan_sil_reserve(void **items,size_t *capacity,size_t needed,size_t size) {
 if(!items||!capacity||!size||needed>SIZE_MAX/size)return false;
 if(needed<=*capacity)return true;
 size_t n=*capacity?*capacity:16;
 while(n<needed){if(n>SIZE_MAX/2){n=needed;break;}n*=2;}
 if(n>SIZE_MAX/size)return false;
 void *p=realloc(*items,n*size);if(!p)return false;*items=p;*capacity=n;return true;
}
static inline void swan_sil_destroy(SwanSILModule *m) {
 if(!m)return;
 for(size_t i=0;i<m->function_count;i++){free(m->functions[i].instructions);free(m->functions[i].blocks);free(m->functions[i].loops);}
 free(m->functions);memset(m,0,sizeof(*m));
}

#endif
