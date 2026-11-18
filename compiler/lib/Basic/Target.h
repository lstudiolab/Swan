#ifndef SWAN_TARGET_H
#define SWAN_TARGET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef enum { SWAN_ARCH_X86_64, SWAN_ARCH_AARCH64, SWAN_ARCH_WASM64 } SwanArchitecture;
typedef enum { SWAN_OBJECT_ELF, SWAN_OBJECT_WASM } SwanObjectFormat;
typedef enum { SWAN_ABI_SYSV, SWAN_ABI_AAPCS64, SWAN_ABI_WASM } SwanABI;
typedef struct { const char *name; const char *triple; SwanArchitecture architecture; SwanObjectFormat object_format; SwanABI abi; bool backend_available; } SwanTarget;
/* These are the only supported Swan code-generation targets. */
static const SwanTarget swan_targets[] = {
 {"x86_64","x86_64-unknown-linux-gnu",SWAN_ARCH_X86_64,SWAN_OBJECT_ELF,SWAN_ABI_SYSV,true},
 {"arm64","aarch64-unknown-linux-gnu",SWAN_ARCH_AARCH64,SWAN_OBJECT_ELF,SWAN_ABI_AAPCS64,true},
 {"wasm64","wasm64-unknown-unknown",SWAN_ARCH_WASM64,SWAN_OBJECT_WASM,SWAN_ABI_WASM,true}
};
#define SWAN_TARGET_COUNT (sizeof(swan_targets)/sizeof(swan_targets[0]))
static inline const SwanTarget *swan_target_find(const char *name){
 if(!name)return NULL;
 if(!strcmp(name,"x86-64")||!strcmp(name,"amd64"))name="x86_64";
 else if(!strcmp(name,"aarch64"))name="arm64";
 for(size_t i=0;i<SWAN_TARGET_COUNT;i++)
  if(!strcmp(name,swan_targets[i].name)||!strcmp(name,swan_targets[i].triple))return &swan_targets[i];
 return NULL;
}
static inline void swan_target_list(FILE *out){
 if(!out)return;
 for(size_t i=0;i<SWAN_TARGET_COUNT;i++)
  fprintf(out,"%-9s %-36s supported\n",swan_targets[i].name,swan_targets[i].triple);
}
#endif
