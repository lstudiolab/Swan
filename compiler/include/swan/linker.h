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
static inline int swan_link_assembly(const char *assembler, const char *input,
                                     const char *output) {
    uint64_t profile_start=swan_profile_start();
    pid_t pid=0;
    char *const args[]={(char *)assembler,"-x","assembler",(char *)input,
                        "-o",(char *)output,NULL};
    int spawn_error=posix_spawnp(&pid,assembler,NULL,NULL,args,environ);
    if(spawn_error!=0){
        swan_profile_end(SWAN_PROFILE_LINK,profile_start);
        errno=spawn_error;
        return -1;
    }

    int status=0;
    pid_t waited;
    do { waited=waitpid(pid,&status,0); } while(waited<0&&errno==EINTR);
    if(waited<0){
        swan_profile_end(SWAN_PROFILE_LINK,profile_start);
        return -1;
    }

    int result=WIFEXITED(status)?WEXITSTATUS(status):
               (WIFSIGNALED(status)?128+WTERMSIG(status):1);
    swan_profile_end(SWAN_PROFILE_LINK,profile_start);
    return result;
}
#endif
