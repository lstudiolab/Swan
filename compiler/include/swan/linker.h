#ifndef SWAN_LINKER_H
#define SWAN_LINKER_H
#include "swan/core.h"
#include <errno.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>

extern char **environ;

/* Host linker driver. posix_spawnp avoids fork-time address-space duplication
 * in the compiler process and lets the platform choose its optimized spawn
 * implementation. Cross-target linking must use an explicit toolchain. */
static inline int swan_link_assembly_with_args(const char *assembler, const char *input,
                                                  const char *output,
                                                  const char *const *extra_args,
                                                  size_t extra_count) {
    uint64_t profile_start=swan_profile_start();
    if(extra_count>120){errno=E2BIG;swan_profile_end(SWAN_PROFILE_LINK,profile_start);return -1;}
    char *args[128];size_t n=0;
    args[n++]=(char *)assembler;args[n++]="-x";args[n++]="assembler";
    args[n++]=(char *)input;args[n++]="-x";args[n++]="none";
    for(size_t i=0;i<extra_count;i++)args[n++]=(char *)extra_args[i];
    args[n++]="-o";args[n++]=(char *)output;args[n]=NULL;
    pid_t pid=0;
    int spawn_error=posix_spawnp(&pid,assembler,NULL,NULL,args,environ);
    if(spawn_error!=0){swan_profile_end(SWAN_PROFILE_LINK,profile_start);errno=spawn_error;return -1;}
    int status=0;pid_t waited;
    do { waited=waitpid(pid,&status,0); } while(waited<0&&errno==EINTR);
    if(waited<0){swan_profile_end(SWAN_PROFILE_LINK,profile_start);return -1;}
    int result=WIFEXITED(status)?WEXITSTATUS(status):
               (WIFSIGNALED(status)?128+WTERMSIG(status):1);
    swan_profile_end(SWAN_PROFILE_LINK,profile_start);
    return result;
}
static inline int swan_link_assembly(const char *assembler,const char *input,const char *output){
    return swan_link_assembly_with_args(assembler,input,output,NULL,0);
}
#endif
