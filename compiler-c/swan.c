#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum { X86_64, AARCH64, ARMV7 } Target;
typedef struct { char *data; size_t len, cap; } Buffer;
static void append(Buffer *b,const char *s) {
    size_t n=strlen(s);
    if(b->len+n+1>b->cap) {
        size_t cap=b->cap?b->cap:256;
        while(cap<b->len+n+1) cap*=2;
        char *p=realloc(b->data,cap);
        if(!p){perror("realloc");exit(2);}
        b->data=p;b->cap=cap;
    }
    memcpy(b->data+b->len,s,n+1);b->len+=n;
}
static char *trim(char *s) {
    while(isspace((unsigned char)*s))s++;
    size_t n=strlen(s);
    while(n&&isspace((unsigned char)s[n-1]))s[--n]=0;
    return s;
}
static int fail(const char *s){fprintf(stderr,"swan: %s\n",s);return 1;}
static Target host_target(void){
#if defined(__aarch64__)
    return AARCH64;
#elif defined(__arm__)
    return ARMV7;
#else
    return X86_64;
#endif
}
static int parse_target(const char *s,Target *t){
    if(!strcmp(s,"native")){*t=host_target();return 1;}
    if(!strcmp(s,"x86_64")||!strcmp(s,"x86-64")){*t=X86_64;return 1;}
    if(!strcmp(s,"aarch64")||!strcmp(s,"arm64")){*t=AARCH64;return 1;}
    if(!strcmp(s,"armv7")||!strcmp(s,"armv7l")){*t=ARMV7;return 1;}
    return 0;
}
static void asm_string(Buffer *b,const char *s,unsigned id){
    char label[40],line[64];
    snprintf(label,sizeof label,".Lswan_str_%u",id);
    snprintf(line,sizeof line,"%s:\n",label);append(b,line);append(b,"  .asciz \"");
    for(;*s;s++){
        char e[8];
        if(*s=='"'||*s=='\\'){e[0]='\\';e[1]=*s;e[2]=0;}
        else if(*s=='\n')strcpy(e,"\\n");
        else if(*s=='\t')strcpy(e,"\\t");
        else if((unsigned char)*s<32)snprintf(e,sizeof e,"\\%03o",(unsigned char)*s);
        else{e[0]=*s;e[1]=0;}
        append(b,e);
    }
    append(b,"\"\n");
}
static int emit_log(Buffer *out,Buffer *strings,Target t,char *v,unsigned *sid){
    if(*v!='"')return fail("log currently requires a string literal");
    char *end=strrchr(v+1,'"');
    if(!end||*trim(end+1))return fail("invalid string literal");
    *end=0;
    char label[40];snprintf(label,sizeof label,".Lswan_str_%u",*sid);
    asm_string(strings,v+1,(*sid)++);
    if(t==X86_64){append(out,"  lea rdi, [rip + ");append(out,label);append(out,"]\n  call puts@PLT\n");}
    else if(t==AARCH64){append(out,"  adrp x0, ");append(out,label);append(out,"\n  add x0, x0, :lo12:");append(out,label);append(out,"\n  bl puts\n");}
    else{append(out,"  ldr r0, =");append(out,label);append(out,"\n  bl puts\n");}
    return 0;
}
static int emit_source(const char *source,Target t,Buffer *out){
    char *copy=strdup(source);if(!copy)return fail("out of memory");
    Buffer strings={0};unsigned sid=0,loop_id=0;
    int in_main=0,have_main=0,returned=0,in_loop=0,loop_indent=0;
    char loop_start[48],loop_end[48];
    const char *header=t==X86_64?".intel_syntax noprefix\n.section .rodata\n":
                       t==AARCH64?".section .rodata\n":".syntax unified\n.section .rodata\n";
    append(out,header);
    char *save=NULL;
    for(char *raw=strtok_r(copy,"\n",&save);raw;raw=strtok_r(NULL,"\n",&save)){
        int indent=0;while(raw[indent]==' '||raw[indent]=='\t')indent+=raw[indent]=='\t'?4:1;
        char *line=trim(raw);if(!*line||!strncmp(line,"# ",2)||!strncmp(line,"//",2))continue;
        if(!strncmp(line,"fn main",7)){
            if(have_main){free(copy);return fail("duplicate main function");}
            have_main=1;in_main=1;
            if(t==X86_64)append(out,".text\n.globl main\n.type main, @function\nmain:\n  push rbp\n  mov rbp, rsp\n  push r12\n  sub rsp, 8\n");
            else if(t==AARCH64)append(out,".text\n.global main\n.type main, %function\nmain:\n  stp x29, x30, [sp, #-16]!\n  mov x29, sp\n  stp x19, x20, [sp, #-16]!\n");
            else append(out,".text\n.global main\n.type main, %function\nmain:\n  push {r4, fp, lr}\n  mov fp, sp\n");
            continue;
        }
        if(!in_main)continue;
        if(in_loop&&indent<=loop_indent){
            if(t==X86_64){append(out,"  inc r12\n  jmp ");append(out,loop_start);append(out,"\n");}
            else if(t==AARCH64){append(out,"  add x19, x19, #1\n  b ");append(out,loop_start);append(out,"\n");}
            else{append(out,"  add r4, r4, #1\n  b ");append(out,loop_start);append(out,"\n");}
            append(out,loop_end);append(out,":\n");in_loop=0;
        }
        if(!strncmp(line,"for ",4)){
            if(in_loop){free(copy);free(strings.data);return fail("nested for loops are not supported yet");}
            char var[64];unsigned long long limit;char tail;
            if(sscanf(line,"for %63s in 0..%llu %c",var,&limit,&tail)!=2||!limit){
                free(copy);free(strings.data);return fail("range loop syntax is: for name in 0..LIMIT");
            }
            if(indent<0){free(copy);free(strings.data);return fail("invalid loop indentation");}
            unsigned id=loop_id++;
            snprintf(loop_start,sizeof loop_start,".Lswan_loop_%u",id);
            snprintf(loop_end,sizeof loop_end,".Lswan_end_%u",id);
            char inst[160];
            if(t==X86_64){
                snprintf(inst,sizeof inst,"  xor r12d, r12d\n%s:\n  cmp r12, %llu\n  jae %s\n",loop_start,limit,loop_end);
            }else if(t==AARCH64){
                snprintf(inst,sizeof inst,"  mov x19, #0\n  ldr x20, =%llu\n%s:\n  cmp x19, x20\n  b.hs %s\n",limit,loop_start,loop_end);
            }else{
                snprintf(inst,sizeof inst,"  mov r4, #0\n  ldr r5, =%llu\n%s:\n  cmp r4, r5\n  bhs %s\n",limit,loop_start,loop_end);
            }
            append(out,inst);in_loop=1;loop_indent=indent;continue;
        }
        if(!strncmp(line,"log ",4)){
            if(emit_log(out,&strings,t,trim(line+4),&sid)){free(copy);free(strings.data);return 1;}
        }else if(!strncmp(line,"return ",7)){
            if(in_loop){free(copy);free(strings.data);return fail("return inside a for loop is not supported yet");}
            char *v=trim(line+7),*end=NULL;long long n=strtoll(v,&end,10);
            if(end==v||*trim(end)){free(copy);free(strings.data);return fail("return requires an integer literal");}
            char inst[96];
            if(t==X86_64)snprintf(inst,sizeof inst,"  mov eax, %lld\n",n);
            else if(t==AARCH64)snprintf(inst,sizeof inst,"  mov x0, #%lld\n",n);
            else snprintf(inst,sizeof inst,"  mov r0, #%lld\n",n);
            append(out,inst);returned=1;
        }else if(!strncmp(line,"pass",4)){
            append(out,"  nop\n");
        }else{
            free(copy);free(strings.data);return fail("unsupported statement (supported: for range loops with log string, log string, return integer, pass)");
        }
        if(in_loop&&indent>loop_indent){
            /* The body is emitted before the loop increment/branch at dedent. */
        }
        if(in_loop&&t==X86_64){
            /* puts may clobber caller-saved registers; r12 is preserved by ABI. */
        }
    }
    if(in_loop){
        append(out,t==X86_64?"  inc r12\n  jmp ":"  add ");
        if(t==X86_64){append(out,loop_start);append(out,"\n");}
        else if(t==AARCH64){append(out,"x19, x19, #1\n  b ");append(out,loop_start);append(out,"\n");}
        else{append(out,"r4, r4, #1\n  b ");append(out,loop_start);append(out,"\n");}
        append(out,loop_end);append(out,":\n");
    }
    if(!have_main){free(copy);free(strings.data);return fail("program must define fn main()");}
    if(!returned){
        append(out,t==X86_64?"  xor eax, eax\n":t==AARCH64?"  mov x0, #0\n":"  mov r0, #0\n");
    }
    if(t==X86_64)append(out,"  add rsp, 8\n  pop r12\n  leave\n  ret\n.size main, .-main\n.section .note.GNU-stack,\"\",@progbits\n");
    else if(t==AARCH64)append(out,"  ldp x19, x20, [sp], #16\n  ldp x29, x30, [sp], #16\n  ret\n.size main, .-main\n");
    else append(out,"  pop {r4, fp, pc}\n.size main, .-main\n");
    if(strings.data){
        Buffer merged={0};append(&merged,header);append(&merged,strings.data);append(&merged,out->data+strlen(header));
        free(out->data);*out=merged;
    }
    free(strings.data);free(copy);return 0;
}
static int run_command(char *const argv[]){
    pid_t pid=fork();if(pid<0){perror("fork");return 1;}
    if(pid==0){execvp(argv[0],argv);perror(argv[0]);_exit(127);}
    int status;while(waitpid(pid,&status,0)<0){if(errno!=EINTR){perror("waitpid");return 1;}}
    return WIFEXITED(status)?WEXITSTATUS(status):1;
}
int main(int argc,char **argv){
    const char *input=NULL,*output=NULL;int emit=0,build=0,showver=0;
    Target target=host_target();
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--version")||!strcmp(argv[i],"-v"))showver=1;
        else if(!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h")){
            puts("Swan C compiler\nUsage: swan [build] file.swan [--target native|x86_64|aarch64|armv7] [--emit-asm] [-o path]");return 0;
        }else if(!strcmp(argv[i],"build"))build=1;
        else if(!strcmp(argv[i],"--emit-asm"))emit=1;
        else if(!strcmp(argv[i],"--target")&&i+1<argc){if(!parse_target(argv[++i],&target))return fail("unknown target; choose native, x86_64, aarch64, or armv7");}
        else if(!strcmp(argv[i],"-o")||!strcmp(argv[i],"--output")){if(++i>=argc)return fail("missing output path");output=argv[i];}
        else if(argv[i][0]=='-')return fail("unknown option");
        else if(!input)input=argv[i];else return fail("unexpected extra argument");
    }
    if(showver){puts("Swan C compiler 0.2.0");return 0;}
    if(!input)return fail("missing input .swan file");
    FILE *f=fopen(input,"rb");if(!f){perror(input);return 1;}
    if(fseek(f,0,SEEK_END)||ftell(f)<0){fclose(f);return fail("cannot size source file");}
    long sz=ftell(f);rewind(f);char *src=malloc((size_t)sz+1);
    if(!src){fclose(f);return fail("out of memory");}
    size_t got=fread(src,1,(size_t)sz,f);fclose(f);src[got]=0;
    Buffer assembly={0};if(emit_source(src,target,&assembly)){free(src);free(assembly.data);return 1;}free(src);
    if(!output)output=emit?"a.s":"a.out";
    if(emit||(strlen(output)>2&&!strcmp(output+strlen(output)-2,".s"))){
        FILE *o=fopen(output,"wb");if(!o){perror(output);free(assembly.data);return 1;}
        fwrite(assembly.data,1,assembly.len,o);fclose(o);free(assembly.data);return 0;
    }
    char *tmp=strdup(output);if(!tmp){free(assembly.data);return fail("out of memory");}
    size_t n=strlen(tmp);if(n+3>=4096){free(tmp);free(assembly.data);return fail("output path too long");}
    char asm_path[4096];snprintf(asm_path,sizeof asm_path,"%s.s",tmp);
    FILE *o=fopen(asm_path,"wb");if(!o){perror(asm_path);free(tmp);free(assembly.data);return 1;}
    fwrite(assembly.data,1,assembly.len,o);fclose(o);free(assembly.data);
    const char *cc=getenv("CC");if(!cc||!*cc)cc="cc";
    char *const cmd[]={(char*)cc,"-x","assembler",asm_path,"-o",tmp,NULL};
    int rc=run_command(cmd);unlink(asm_path);
    if(rc){free(tmp);return rc;}
    if(!build){char executable[4096];const char *path=tmp;
        if(!strchr(tmp,'/')){snprintf(executable,sizeof executable,"./%s",tmp);path=executable;}
        char *const run[]={ (char*)path,NULL };rc=run_command(run);
    }
    free(tmp);return rc;
}
