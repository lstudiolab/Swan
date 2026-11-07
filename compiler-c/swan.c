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

static void append(Buffer *b, const char *s) {
    size_t n = strlen(s);
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 256;
        while (cap < b->len + n + 1) cap *= 2;
        char *p = realloc(b->data, cap);
        if (!p) { perror("realloc"); exit(2); }
        b->data = p; b->cap = cap;
    }
    memcpy(b->data + b->len, s, n + 1); b->len += n;
}
static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n-1])) s[--n] = 0;
    return s;
}
static int fail(const char *message) { fprintf(stderr, "swan: %s\n", message); return 1; }
static Target host_target(void) {
#if defined(__aarch64__)
    return AARCH64;
#elif defined(__arm__)
    return ARMV7;
#else
    return X86_64;
#endif
}
static int parse_target(const char *s, Target *t) {
    if (!strcmp(s,"native")) { *t=host_target(); return 1; }
    if (!strcmp(s,"x86_64") || !strcmp(s,"x86-64")) { *t=X86_64; return 1; }
    if (!strcmp(s,"aarch64") || !strcmp(s,"arm64")) { *t=AARCH64; return 1; }
    if (!strcmp(s,"armv7") || !strcmp(s,"armv7l")) { *t=ARMV7; return 1; }
    return 0;
}
static void asm_string(Buffer *out, const char *s, unsigned id) {
    char label[40]; snprintf(label,sizeof label,".Lswan_str_%u",id);
    if (strstr(out->data ? out->data : "", label)) return;
    char line[128]; snprintf(line,sizeof line,"%s:\n",label); append(out,line);
    append(out,"  .asciz \"");
    for (; *s; s++) {
        char esc[8];
        if (*s=='"' || *s=='\\') { esc[0]='\\'; esc[1]=*s; esc[2]=0; }
        else if (*s=='\n') strcpy(esc,"\\n");
        else if (*s=='\t') strcpy(esc,"\\t");
        else if ((unsigned char)*s < 32) { snprintf(esc,sizeof esc,"\\%03o",(unsigned char)*s); }
        else { esc[0]=*s; esc[1]=0; }
        append(out,esc);
    }
    append(out,"\"\n");
}
static int emit_source(const char *source, Target target, Buffer *out) {
    char *copy=strdup(source); if(!copy) return fail("out of memory");
    Buffer strings={0}; unsigned sid=0; int in_main=0, have_main=0, returned=0;
    append(out, target==X86_64 ? ".intel_syntax noprefix\n.section .rodata\n" :
                 target==AARCH64 ? ".section .rodata\n" : ".syntax unified\n.section .rodata\n");
    char *save=NULL;
    for(char *raw=strtok_r(copy,"\n",&save); raw; raw=strtok_r(NULL,"\n",&save)) {
        char *line=trim(raw);
        if(!*line || !strncmp(line,"# ",2) || !strncmp(line,"//",2) || !strncmp(line,"struct ",7)) continue;
        if(!strncmp(line,"fn main",7)) {
            if(have_main) { free(copy); return fail("duplicate main function"); }
            have_main=1; in_main=1; returned=0;
            append(out,target==X86_64?".text\n.globl main\n.type main, @function\nmain:\n  push rbp\n  mov rbp, rsp\n":
                 target==AARCH64?".text\n.global main\n.type main, %function\nmain:\n  stp x29, x30, [sp, #-16]!\n  mov x29, sp\n":
                 ".text\n.global main\n.type main, %function\nmain:\n  push {fp, lr}\n  mov fp, sp\n");
            continue;
        }
        if(!in_main) continue;
        if(!strncmp(line,"log ",4)) {
            char *v=trim(line+4);
            if(*v=='"') {
                char *end=strrchr(v+1,'"'); if(!end) {free(copy);return fail("unterminated string literal");}
                *end=0; char label[40]; snprintf(label,sizeof label,".Lswan_str_%u",sid);
                asm_string(&strings,v+1,sid++);
                if(target==X86_64) { append(out,"  lea rdi, [rip + "); append(out,label); append(out,"]\n  call puts@PLT\n"); }
                else if(target==AARCH64) { append(out,"  adrp x0, "); append(out,label); append(out,"\n  add x0, x0, :lo12:"); append(out,label); append(out,"\n  bl puts\n"); }
                else { append(out,"  ldr r0, ="); append(out,label); append(out,"\n  bl puts\n"); }
            } else {
                free(copy); return fail("C migration currently accepts string log expressions; integer expression lowering is being ported next");
            }
        } else if(!strncmp(line,"return ",7)) {
            char *v=trim(line+7); char *end=NULL; long long n=strtoll(v,&end,10);
            if(end==v || *trim(end)) {free(copy);return fail("return currently requires an integer literal");}
            char inst[96];
            if(target==X86_64) snprintf(inst,sizeof inst,"  mov eax, %lld\n",n);
            else if(target==AARCH64) snprintf(inst,sizeof inst,"  mov x0, #%lld\n",n);
            else snprintf(inst,sizeof inst,"  mov r0, #%lld\n",n);
            append(out,inst); returned=1;
        } else if(!strncmp(line,"pass",4)) {
            append(out,target==X86_64?"  nop\n":target==AARCH64?"  nop\n":"  nop\n");
        } else {
            free(copy); return fail("unsupported statement in initial C compiler backend (supported: log string, return integer, pass)");
        }
    }
    if(!have_main) { free(copy); return fail("program must define fn main()"); }
    if(!returned) {
        append(out,target==X86_64?"  xor eax, eax\n":target==AARCH64?"  mov x0, #0\n":"  mov r0, #0\n");
    }
    append(out,target==X86_64?"  leave\n  ret\n.size main, .-main\n.section .note.GNU-stack,\"\",@progbits\n":
                 target==AARCH64?"  ldp x29, x30, [sp], #16\n  ret\n.size main, .-main\n":
                 "  pop {fp, pc}\n.size main, .-main\n");
    /* String literals are emitted before text so references are position independent. */
    if(strings.data) {
        size_t pos=strlen(target==X86_64?".intel_syntax noprefix\n.section .rodata\n":target==AARCH64?".section .rodata\n":".syntax unified\n.section .rodata\n");
        Buffer merged={0}; append(&merged,target==X86_64?".intel_syntax noprefix\n.section .rodata\n":target==AARCH64?".section .rodata\n":".syntax unified\n.section .rodata\n"); append(&merged,strings.data); append(&merged,out->data+pos);
        free(out->data); *out=merged;
    }
    free(strings.data); free(copy); return 0;
}
static int run_command(char *const argv[]) {
    pid_t pid=fork(); if(pid<0){perror("fork");return 1;}
    if(pid==0){execvp(argv[0],argv);perror(argv[0]);_exit(127);}
    int status; while(waitpid(pid,&status,0)<0){if(errno!=EINTR){perror("waitpid");return 1;}}
    return WIFEXITED(status)?WEXITSTATUS(status):1;
}
int main(int argc,char **argv) {
    const char *input=NULL,*output=NULL; int emit=0,build=0,showver=0;
    Target target=host_target(); int target_set=0;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--version")||!strcmp(argv[i],"-v")) showver=1;
        else if(!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h")) {
            puts("Swan C compiler\nUsage: swan [build] file.swan [--target native|x86_64|aarch64|armv7] [--emit-asm] [-o path]");
            return 0;
        } else if(!strcmp(argv[i],"build")) build=1;
        else if(!strcmp(argv[i],"--emit-asm")) emit=1;
        else if(!strcmp(argv[i],"--target")&&i+1<argc) {if(!parse_target(argv[++i],&target))return fail("unknown target; choose native, x86_64, aarch64, or armv7");target_set=1;}
        else if(!strcmp(argv[i],"-o")||!strcmp(argv[i],"--output")) {if(++i>=argc)return fail("missing output path");output=argv[i];}
        else if(argv[i][0]=='-') return fail("unknown option");
        else if(!input) input=argv[i]; else return fail("unexpected extra argument");
    }
    if(showver){puts("Swan C compiler 0.1.0");return 0;}
    if(!input)return fail("missing input .swan file");
    FILE *f=fopen(input,"rb"); if(!f){perror(input);return 1;}
    if(fseek(f,0,SEEK_END)||ftell(f)<0){fclose(f);return fail("cannot size source file");}
    long sz=ftell(f); rewind(f); char *src=malloc((size_t)sz+1);
    if(!src){fclose(f);return fail("out of memory");}
    size_t got=fread(src,1,(size_t)sz,f); fclose(f); src[got]=0;
    Buffer assembly={0}; if(emit_source(src,target,&assembly)){free(src);free(assembly.data);return 1;} free(src);
    if(!output) output=emit? "a.s" : "a.out";
    if(emit || (strlen(output)>2&&!strcmp(output+strlen(output)-2,".s"))) {
        FILE *o=fopen(output,"wb"); if(!o){perror(output);free(assembly.data);return 1;}
        fwrite(assembly.data,1,assembly.len,o); fclose(o);
        free(assembly.data); return 0;
    }
    char *tmp=strdup(output); if(!tmp){free(assembly.data);return fail("out of memory");}
    size_t n=strlen(tmp); if(n+3>=4096){free(tmp);free(assembly.data);return fail("output path too long");}
    char asm_path[4096]; snprintf(asm_path,sizeof asm_path,"%s.s",tmp);
    FILE *o=fopen(asm_path,"wb"); if(!o){perror(asm_path);free(tmp);free(assembly.data);return 1;}
    fwrite(assembly.data,1,assembly.len,o); fclose(o); free(assembly.data);
    const char *cc=getenv("CC"); if(!cc||!*cc) cc="cc";
    char *const cmd[]={(char*)cc,"-x","assembler",asm_path,"-o",tmp,NULL};
    int rc=run_command(cmd); unlink(asm_path);
    if(rc){free(tmp);return rc;}
    if(!build) {char *const run[]={tmp,NULL}; rc=run_command(run);}
    free(tmp); (void)target_set; return rc;
}
