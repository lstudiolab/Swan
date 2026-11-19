#ifndef BREW_TARGET_H
#define BREW_TARGET_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef enum { BREW_ARCH_X86_64, BREW_ARCH_AARCH64, BREW_ARCH_WASM64 } BrewArchitecture;
typedef enum { BREW_OBJECT_ELF, BREW_OBJECT_WASM } BrewObjectFormat;
typedef enum { BREW_ABI_SYSV, BREW_ABI_AAPCS64, BREW_ABI_WASM } BrewABI;
typedef struct { const char *name; const char *triple; BrewArchitecture architecture; BrewObjectFormat object_format; BrewABI abi; bool backend_available; } BrewTarget;
/* These are the only supported Brew code-generation targets. */
static const BrewTarget brew_targets[] = {
 {"x86_64","x86_64-unknown-linux-gnu",BREW_ARCH_X86_64,BREW_OBJECT_ELF,BREW_ABI_SYSV,true},
 {"arm64","aarch64-unknown-linux-gnu",BREW_ARCH_AARCH64,BREW_OBJECT_ELF,BREW_ABI_AAPCS64,true},
 {"wasm64","wasm64-unknown-unknown",BREW_ARCH_WASM64,BREW_OBJECT_WASM,BREW_ABI_WASM,true}
};
#define BREW_TARGET_COUNT (sizeof(brew_targets)/sizeof(brew_targets[0]))
static inline const BrewTarget *brew_target_find(const char *name){
 if(!name)return NULL;
 if(!strcmp(name,"x86-64")||!strcmp(name,"amd64"))name="x86_64";
 else if(!strcmp(name,"aarch64"))name="arm64";
 for(size_t i=0;i<BREW_TARGET_COUNT;i++)
  if(!strcmp(name,brew_targets[i].name)||!strcmp(name,brew_targets[i].triple))return &brew_targets[i];
 return NULL;
}
static inline void brew_target_list(FILE *out){
 if(!out)return;
 for(size_t i=0;i<BREW_TARGET_COUNT;i++)
  fprintf(out,"%-9s %-36s supported\n",brew_targets[i].name,brew_targets[i].triple);
}
#endif
