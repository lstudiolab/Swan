#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef enum { X86_64, AARCH64, ARMV7 } Target;
typedef struct { char *data; size_t len, cap; } Buffer;
typedef struct { char name[64]; char start[48], step_label[48], end[48]; int indent, index_slot, limit_slot, has_body, reg_pair; } Loop;

static void append_n(Buffer *b,const char *s,size_t n) {
    if(b->len+n+1>b->cap) {
        size_t cap=b->cap?b->cap:256;
        while(cap<b->len+n+1) cap*=2;
        char *p=realloc(b->data,cap);
        if(!p){perror("realloc");exit(2);}
        b->data=p;b->cap=cap;
    }
    memcpy(b->data+b->len,s,n);b->len+=n;b->data[b->len]=0;
}
static void append(Buffer *b,const char *s){append_n(b,s,strlen(s));}
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
static void asm_string(Buffer *b,const char *s,const char *label){
    append(b,label);append(b,":\n  .asciz \"");
    for(;*s;s++){
        char e[8];
        if(*s=='"'||*s=='\\'){e[0]='\\';e[1]=*s;e[2]=0;}
        else if(*s=='\n')strcpy(e,"\n");
        else if(*s=='\t')strcpy(e,"\\t");
        else if((unsigned char)*s<32)snprintf(e,sizeof e,"\\%03o",(unsigned char)*s);
        else{e[0]=*s;e[1]=0;}
        append(b,e);
    }
    append(b,"\"\n");
}
static int identifier(const char *s){
    if(!s||!(isalpha((unsigned char)*s)||*s=='_'))return 0;
    for(s++;*s;s++)if(!(isalnum((unsigned char)*s)||*s=='_'))return 0;
    return 1;
}
static Loop *find_loop(Loop *loops,int depth,const char *name){
    for(int i=depth-1;i>=0;i--)if(!strcmp(loops[i].name,name))return &loops[i];
    return NULL;
}
static void emit_store_const(Buffer *b,Target t,int slot,long long value){
    char s[256];
    if(t==X86_64){
        snprintf(s,sizeof s,"  movabs rax, %lld\n  mov qword ptr [rbp - %d], rax\n",value,slot);append(b,s);
    }else if(t==AARCH64){
        snprintf(s,sizeof s,"  ldr x10, =%lld\n  sub x9, x29, #%d\n  str x10, [x9]\n",value,slot);append(b,s);
    }else{
        snprintf(s,sizeof s,"  ldr r0, =%lld\n  sub r2, r11, #%d\n  str r0, [r2]\n",value,slot);append(b,s);
    }
}
static void emit_loop_open(Buffer *b,Target t,const Loop *l,long long first,long long limit){
    if(t==X86_64&&l->reg_pair){
        const char *index=l->reg_pair==1?"r12":"r14",*limit_reg=l->reg_pair==1?"r13":"r15";
        char s[220];
        snprintf(s,sizeof s,"  movabs %s, %lld\n  movabs %s, %lld\n",index,first,limit_reg,limit);
        append(b,s);append(b,l->start);append(b,":\n");
        snprintf(s,sizeof s,"  cmp %s, %s\n  jge %s\n",index,limit_reg,l->end);append(b,s);
        return;
    }
    emit_store_const(b,t,l->index_slot,first);
    emit_store_const(b,t,l->limit_slot,limit);
    append(b,l->start);append(b,":\n");
    if(t==X86_64){
        char s[160];snprintf(s,sizeof s,"  mov rax, qword ptr [rbp - %d]\n  cmp rax, qword ptr [rbp - %d]\n  jge %s\n",l->index_slot,l->limit_slot,l->end);append(b,s);
    }else if(t==AARCH64){
        char s[200];snprintf(s,sizeof s,"  sub x9, x29, #%d\n  ldr x10, [x9]\n  sub x9, x29, #%d\n  ldr x11, [x9]\n  cmp x10, x11\n  b.ge %s\n",l->index_slot,l->limit_slot,l->end);append(b,s);
    }else{
        char s[200];snprintf(s,sizeof s,"  sub r2, r11, #%d\n  ldr r0, [r2]\n  sub r2, r11, #%d\n  ldr r1, [r2]\n  cmp r0, r1\n  bge %s\n",l->index_slot,l->limit_slot,l->end);append(b,s);
    }
}
static void emit_loop_close(Buffer *b,Target t,const Loop *l){
    append(b,l->step_label);append(b,":\n");
    if(t==X86_64){
        char s[160];
        if(l->reg_pair){const char *index=l->reg_pair==1?"r12":"r14";snprintf(s,sizeof s,"  inc %s\n  jmp %s\n%s:\n",index,l->start,l->end);}
        else snprintf(s,sizeof s,"  add qword ptr [rbp - %d], 1\n  jmp %s\n%s:\n",l->index_slot,l->start,l->end);
        append(b,s);
    }else if(t==AARCH64){
        char s[200];snprintf(s,sizeof s,"  sub x9, x29, #%d\n  ldr x10, [x9]\n  add x10, x10, #1\n  str x10, [x9]\n  b %s\n%s:\n",l->index_slot,l->start,l->end);append(b,s);
    }else{
        char s[200];snprintf(s,sizeof s,"  sub r2, r11, #%d\n  ldr r0, [r2]\n  add r0, r0, #1\n  str r0, [r2]\n  b %s\n%s:\n",l->index_slot,l->start,l->end);append(b,s);
    }
}
static int emit_log(Buffer *code,Buffer *data,Target t,char *value,Loop *loops,int depth,unsigned *string_id,int *needs_int){
    if(*value=='"'){
        char *end=strrchr(value+1,'"');
        if(!end||*trim(end+1))return fail("invalid string literal");
        *end=0;
        char label[48];snprintf(label,sizeof label,".Lswan_str_%u",(*string_id)++);
        asm_string(data,value+1,label);
        if(t==X86_64){append(code,"  lea rdi, [rip + ");append(code,label);append(code,"]\n  call puts@PLT\n");}
        else if(t==AARCH64){append(code,"  adrp x0, ");append(code,label);append(code,"\n  add x0, x0, :lo12:");append(code,label);append(code,"\n  bl puts\n");}
        else{append(code,"  ldr r0, =");append(code,label);append(code,"\n  bl puts\n");}
        return 0;
    }
    if(!identifier(value))return fail("log accepts a string literal or an active for-loop variable");
    Loop *loop=find_loop(loops,depth,value);
    if(!loop)return fail("unknown variable in log statement");
    *needs_int=1;
    if(t==X86_64){
        char s[200];
        if(loop->reg_pair){const char *index=loop->reg_pair==1?"r12":"r14";snprintf(s,sizeof s,"  mov rsi, %s\n  lea rdi, [rip + .Lswan_int_fmt]\n  xor eax, eax\n  call printf@PLT\n",index);}
        else snprintf(s,sizeof s,"  mov rsi, qword ptr [rbp - %d]\n  lea rdi, [rip + .Lswan_int_fmt]\n  xor eax, eax\n  call printf@PLT\n",loop->index_slot);
        append(code,s);
    }else if(t==AARCH64){char s[180];snprintf(s,sizeof s,"  sub x9, x29, #%d\n  ldr x1, [x9]\n  adrp x0, .Lswan_int_fmt\n  add x0, x0, :lo12:.Lswan_int_fmt\n  bl printf\n",loop->index_slot);append(code,s);}
    else{char s[160];snprintf(s,sizeof s,"  sub r2, r11, #%d\n  ldr r1, [r2]\n  ldr r0, =.Lswan_int_fmt\n  bl printf\n",loop->index_slot);append(code,s);}
    return 0;
}
static int emit_source(const char *source,Target t,Buffer *out){
    char *copy=strdup(source);if(!copy)return fail("out of memory");
    Buffer code={0},data={0};Loop loops[128];int depth=0,max_depth=0,in_main=0,have_main=0,returned=0,needs_int=0;
    unsigned loop_id=0,string_id=0;char *save=NULL;
    for(char *raw=strtok_r(copy,"\n",&save);raw;raw=strtok_r(NULL,"\n",&save)){
        int indent=0;while(raw[indent]==' '||raw[indent]=='\t')indent+=raw[indent]=='\t'?4:1;
        char *line=trim(raw);
        if(!*line||!strncmp(line,"//",2)||!strncmp(line,"# ",2))continue;
        if(!in_main){
            if(!strncmp(line,"fn main",7)){
                if(have_main){free(copy);return fail("duplicate main function");}
                have_main=1;in_main=1;continue;
            }
            continue;
        }
        while(depth&&indent<=loops[depth-1].indent){
            if(!loops[depth-1].has_body){free(copy);free(code.data);free(data.data);return fail("for loop body cannot be empty");}
            emit_loop_close(&code,t,&loops[depth-1]);depth--;
        }
        if(!strncmp(line,"for ",4)){
            if(depth>=128){free(copy);free(code.data);free(data.data);return fail("maximum nested loop depth is 128");}
            if(depth&&indent<=loops[depth-1].indent){free(copy);free(code.data);free(data.data);return fail("invalid loop indentation");}
            char name[64],extra;long long first,last;
            if(sscanf(line,"for %63s in %lld..%lld %c",name,&first,&last,&extra)!=3||!identifier(name)){
                free(copy);free(code.data);free(data.data);return fail("range syntax: for name in START..END (END is exclusive)");
            }
            if(t==ARMV7&&(first<INT32_MIN||first>INT32_MAX||last<INT32_MIN||last>INT32_MAX)){
                free(copy);free(code.data);free(data.data);return fail("ARMv7 range bounds must fit signed 32-bit integers");
            }
            if(depth)loops[depth-1].has_body=1;
            Loop *l=&loops[depth];memset(l,0,sizeof(*l));strcpy(l->name,name);l->indent=indent;
            int spill_depth=(t==X86_64&&depth>=2)?depth-2:depth;
            l->reg_pair=(t==X86_64&&depth<2)?depth+1:0;
            l->index_slot=(spill_depth*2+1)*8;l->limit_slot=(spill_depth*2+2)*8;
            snprintf(l->start,sizeof l->start,".Lswan_loop_%u",loop_id);
            snprintf(l->step_label,sizeof l->step_label,".Lswan_step_%u",loop_id);snprintf(l->end,sizeof l->end,".Lswan_end_%u",loop_id++);
            emit_loop_open(&code,t,l,first,last);depth++;if(depth>max_depth)max_depth=depth;continue;
        }
        if(depth)loops[depth-1].has_body=1;
        if(!strcmp(line,"break")){
            if(!depth){free(copy);free(code.data);free(data.data);return fail("break outside a loop");}
            append(&code,"  jmp ");append(&code,loops[depth-1].end);append(&code,"\n");
        }else if(!strcmp(line,"continue")){
            if(!depth){free(copy);free(code.data);free(data.data);return fail("continue outside a loop");}
            append(&code,"  jmp ");append(&code,loops[depth-1].step_label);append(&code,"\n");
        }else if(!strncmp(line,"log ",4)){
            if(emit_log(&code,&data,t,trim(line+4),loops,depth,&string_id,&needs_int)){free(copy);free(code.data);free(data.data);return 1;}
        }else if(!strncmp(line,"return ",7)){
            char *v=trim(line+7),*end=NULL;long long n=strtoll(v,&end,10);
            if(end==v||*trim(end)){free(copy);free(code.data);free(data.data);return fail("return requires an integer literal");}
            if(t==ARMV7&&(n<INT32_MIN||n>INT32_MAX)){free(copy);free(code.data);free(data.data);return fail("ARMv7 return value must fit signed 32-bit integer");}
            char s[128];
            if(t==X86_64)snprintf(s,sizeof s,"  mov eax, %lld\n  jmp .Lswan_exit\n",n);
            else if(t==AARCH64)snprintf(s,sizeof s,"  ldr x0, =%lld\n  b .Lswan_exit\n",n);
            else snprintf(s,sizeof s,"  ldr r0, =%lld\n  b .Lswan_exit\n",n);
            append(&code,s);returned=1;
        }else if(!strcmp(line,"pass")){
            append(&code,"  nop\n");
        }else{
            free(copy);free(code.data);free(data.data);return fail("unsupported statement (supported: nested range loops, break/continue, log string/loop variable, return integer, pass)");
        }
    }
    while(depth){
        if(!loops[depth-1].has_body){free(copy);free(code.data);free(data.data);return fail("for loop body cannot be empty");}
        emit_loop_close(&code,t,&loops[depth-1]);depth--;
    }
    if(!have_main){free(copy);free(code.data);free(data.data);return fail("program must define fn main()");}
    if(needs_int)asm_string(&data,t==ARMV7?"%d\n":"%lld\n",".Lswan_int_fmt");
    int frame_size=(t==X86_64&&max_depth>2)?(max_depth-2)*16:max_depth*16;
    int saved_pairs=t==X86_64?(max_depth>1?2:max_depth):0;
    char prologue[160],epilogue[160];
    if(t==X86_64){
        append(out,".intel_syntax noprefix\n.section .rodata\n");append_n(out,data.data?data.data:"",data.len);
        append(out,".text\n.globl main\n.type main, @function\nmain:\n  push rbp\n  mov rbp, rsp\n");
        if(saved_pairs>=1)append(out,"  push r12\n  push r13\n");
        if(saved_pairs>=2)append(out,"  push r14\n  push r15\n");
        if(frame_size){snprintf(prologue,sizeof prologue,"  sub rsp, %d\n",frame_size);append(out,prologue);}
        append_n(out,code.data?code.data:"",code.len);
        if(!returned)append(out,"  xor eax, eax\n");
        append(out,".Lswan_exit:\n");
        if(frame_size){snprintf(epilogue,sizeof epilogue,"  add rsp, %d\n",frame_size);append(out,epilogue);}
        if(saved_pairs>=2)append(out,"  pop r15\n  pop r14\n");
        if(saved_pairs>=1)append(out,"  pop r13\n  pop r12\n");
        append(out,"  leave\n  ret\n.size main, .-main\n.section .note.GNU-stack,\"\",@progbits\n");
    }else if(t==AARCH64){
        append(out,".section .rodata\n");append_n(out,data.data?data.data:"",data.len);
        append(out,".text\n.global main\n.type main, %function\nmain:\n  stp x29, x30, [sp, #-16]!\n  mov x29, sp\n");
        if(frame_size){snprintf(prologue,sizeof prologue,"  sub sp, sp, #%d\n",frame_size);append(out,prologue);}
        append_n(out,code.data?code.data:"",code.len);
        if(!returned)append(out,"  mov x0, #0\n");
        append(out,".Lswan_exit:\n");
        if(frame_size){snprintf(epilogue,sizeof epilogue,"  add sp, sp, #%d\n",frame_size);append(out,epilogue);}
        append(out,"  ldp x29, x30, [sp], #16\n  ret\n.size main, .-main\n");
    }else{
        append(out,".syntax unified\n.section .rodata\n");append_n(out,data.data?data.data:"",data.len);
        append(out,".text\n.global main\n.type main, %function\nmain:\n  push {r4, r5, r6, r7, r11, lr}\n  mov r11, sp\n");
        if(frame_size){snprintf(prologue,sizeof prologue,"  ldr r12, =%d\n  sub sp, sp, r12\n",frame_size);append(out,prologue);}
        append_n(out,code.data?code.data:"",code.len);
        if(!returned)append(out,"  mov r0, #0\n");
        append(out,".Lswan_exit:\n");
        if(frame_size){snprintf(epilogue,sizeof epilogue,"  ldr r12, =%d\n  add sp, sp, r12\n",frame_size);append(out,epilogue);}
        append(out,"  pop {r4, r5, r6, r7, r11, pc}\n.size main, .-main\n");
    }
    free(copy);free(code.data);free(data.data);return 0;
}
static int run_command(char *const argv[]){
    pid_t pid=fork();if(pid<0){perror("fork");return 1;}
    if(pid==0){execvp(argv[0],argv);perror(argv[0]);_exit(127);}
    int status;while(waitpid(pid,&status,0)<0){if(errno!=EINTR){perror("waitpid");return 1;}}
    return WIFEXITED(status)?WEXITSTATUS(status):1;
}
int main(int argc,char **argv){
    const char *input=NULL,*output=NULL;int emit=0,build=0,showver=0;Target target=host_target();
    for(int i=1;i<argc;i++){
        if(!strcmp(argv[i],"--version")||!strcmp(argv[i],"-v"))showver=1;
        else if(!strcmp(argv[i],"--help")||!strcmp(argv[i],"-h")){puts("Swan C compiler\nUsage: swan [build] file.swan [--target native|x86_64|aarch64|armv7] [--emit-asm] [-o path]");return 0;}
        else if(!strcmp(argv[i],"build"))build=1;
        else if(!strcmp(argv[i],"--emit-asm"))emit=1;
        else if(!strcmp(argv[i],"--target")&&i+1<argc){if(!parse_target(argv[++i],&target))return fail("unknown target; choose native, x86_64, aarch64, or armv7");}
        else if(!strcmp(argv[i],"-o")||!strcmp(argv[i],"--output")){if(++i>=argc)return fail("missing output path");output=argv[i];}
        else if(argv[i][0]=='-')return fail("unknown option");
        else if(!input)input=argv[i];else return fail("unexpected extra argument");
    }
    if(showver){puts("Swan C compiler 0.3.0");return 0;}
    if(!input)return fail("missing input .swan file");
    FILE *f=fopen(input,"rb");if(!f){perror(input);return 1;}
    if(fseek(f,0,SEEK_END)||ftell(f)<0){fclose(f);return fail("cannot size source file");}
    long size=ftell(f);rewind(f);char *src=malloc((size_t)size+1);
    if(!src){fclose(f);return fail("out of memory");}
    size_t got=fread(src,1,(size_t)size,f);fclose(f);src[got]=0;
    Buffer assembly={0};if(emit_source(src,target,&assembly)){free(src);free(assembly.data);return 1;}free(src);
    if(!output)output=emit?"a.s":"a.out";
    if(emit||(strlen(output)>2&&!strcmp(output+strlen(output)-2,".s"))){
        FILE *o=fopen(output,"wb");if(!o){perror(output);free(assembly.data);return 1;}
        fwrite(assembly.data,1,assembly.len,o);fclose(o);free(assembly.data);return 0;
    }
    char *tmp=strdup(output);if(!tmp){free(assembly.data);return fail("out of memory");}
    if(strlen(tmp)+3>=4096){free(tmp);free(assembly.data);return fail("output path too long");}
    char asm_path[4096];snprintf(asm_path,sizeof asm_path,"%s.s",tmp);
    FILE *o=fopen(asm_path,"wb");if(!o){perror(asm_path);free(tmp);free(assembly.data);return 1;}
    fwrite(assembly.data,1,assembly.len,o);fclose(o);free(assembly.data);
    const char *cc=getenv("CC");if(!cc||!*cc)cc="cc";
    char *const cmd[]={(char*)cc,"-x","assembler",asm_path,"-o",tmp,NULL};
    int rc=run_command(cmd);unlink(asm_path);
    if(rc){free(tmp);return rc;}
    if(!build){char executable[4096];const char *path=tmp;if(!strchr(tmp,'/')){snprintf(executable,sizeof executable,"./%s",tmp);path=executable;}char *const run[]={ (char*)path,NULL };rc=run_command(run);}
    free(tmp);return rc;
}
