#ifndef SWAN_LINKER_H
#define SWAN_LINKER_H
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Host linker driver. Cross-target linking must be selected by an explicit
 * target toolchain; this helper never silently links with the host ABI. */
static inline int swan_link_assembly(const char *assembler, const char *input,
                                     const char *output) {
    uint64_t profile_start=swan_profile_start();
    pid_t pid=fork();
    if(pid<0){swan_profile_end(SWAN_PROFILE_LINK,profile_start);return -1;}
    if(pid==0){execlp(assembler,assembler,"-x","assembler",input,"-o",output,(char*)NULL);_exit(127);}
    int status=0;
    while(waitpid(pid,&status,0)<0){if(errno==EINTR)continue;return -1;}
    int result=WIFEXITED(status)?WEXITSTATUS(status):(WIFSIGNALED(status)?128+WTERMSIG(status):1);
    swan_profile_end(SWAN_PROFILE_LINK,profile_start);return result;
}
#endif
