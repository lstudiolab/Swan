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
    pid_t pid=fork();
    if(pid<0)return -1;
    if(pid==0){execlp(assembler,assembler,"-x","assembler",input,"-o",output,(char*)NULL);_exit(127);}
    int status=0;
    while(waitpid(pid,&status,0)<0){if(errno==EINTR)continue;return -1;}
    if(WIFEXITED(status))return WEXITSTATUS(status);
    if(WIFSIGNALED(status))return 128+WTERMSIG(status);
    return 1;
}
#endif
