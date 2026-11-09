#ifndef SWAN_C_HEADER_H
#define SWAN_C_HEADER_H
#include "swan/parser.h"
#include <sys/types.h>
#include <sys/wait.h>

/* The first C-header importer deliberately accepts only scalar ABI types
 * that the current x86-64 backend can pass without layout conversion. */
typedef struct {
    char name[SWAN_MAX_TEXT];
    const char *return_type;
    const char *parameter_types[16];
    size_t parameter_count;
    bool eligible, has_body;
} SwanCHeaderDecl;

static inline const char *swan_c_header_scalar_type(const char *type) {
    if (!type) return NULL;
    while (*type == ' ' || *type == '\t') type++;
    if (!strncmp(type,"const ",6)) type += 6;
    if (!strncmp(type,"volatile ",9)) type += 9;
    if (!strcmp(type,"void")) return "void";
    if (!strcmp(type,"_Bool") || !strcmp(type,"bool")) return "bool";
    if (!strcmp(type,"int") || !strcmp(type,"signed int") || !strcmp(type,"int32_t")) return "i32";
    if (!strcmp(type,"unsigned int") || !strcmp(type,"uint32_t")) return "i64";
    if (!strcmp(type,"int64_t") || !strcmp(type,"uint64_t") ||
        !strcmp(type,"long") || !strcmp(type,"long int") ||
        !strcmp(type,"signed long") || !strcmp(type,"signed long int") ||
        !strcmp(type,"unsigned long") || !strcmp(type,"unsigned long int") ||
        !strcmp(type,"long long") || !strcmp(type,"long long int") ||
        !strcmp(type,"signed long long") || !strcmp(type,"unsigned long long") ||
        !strcmp(type,"size_t") || !strcmp(type,"ssize_t") ||
        !strcmp(type,"intptr_t") || !strcmp(type,"uintptr_t") ||
        !strcmp(type,"ptrdiff_t")) return "i64";
    return NULL;
}

static inline bool swan_c_header_copy(char *out,size_t cap,const char *begin,size_t length) {
    while(length && (begin[length-1]==' ' || begin[length-1]=='\t')) length--;
    while(length && (*begin==' ' || *begin=='\t')) { begin++; length--; }
    if(!length || length>=cap) return false;
    memcpy(out,begin,length);out[length]=0;return true;
}

static inline bool swan_c_header_finish_decl(Program *program,SwanCHeaderDecl *decl,
                                              const char *header,SwanError *error) {
    if(!decl->eligible || decl->has_body) return true;
    for(size_t i=0;i<program->count;i++) {
        Stmt *prior=program->functions[i];
        if(strcmp(prior->name,decl->name)) continue;
        if(!prior->external || prior->param_count!=decl->parameter_count ||
           !prior->type || strcmp(prior->type,decl->return_type)) {
            swan_error(error,NULL,"C header '%s' declares conflicting function '%s'",header,decl->name);
            return false;
        }
        for(size_t p=0;p<decl->parameter_count;p++)
            if(!prior->param_types || !prior->param_types[p] ||
               strcmp(prior->param_types[p],decl->parameter_types[p])) {
                swan_error(error,NULL,"C header '%s' declares conflicting signature for '%s'",header,decl->name);
                return false;
            }
        return true;
    }
    if(program->count>=1024) {
        swan_error(error,NULL,"C header import exceeds the compiler function limit");
        return false;
    }
    Stmt *fn=swan_arena_alloc(program,sizeof(*fn));
    char *name=swan_arena_alloc(program,strlen(decl->name)+1);
    char *ret=swan_arena_alloc(program,strlen(decl->return_type)+1);
    const char **params=swan_arena_alloc(program,16*sizeof(*params));
    const char **types=swan_arena_alloc(program,16*sizeof(*types));
    if(!fn||!name||!ret||!params||!types) {
        swan_error(error,NULL,"out of memory importing C header declaration");
        return false;
    }
    strcpy(name,decl->name);strcpy(ret,decl->return_type);
    fn->kind=S_FUNCTION;fn->external=true;fn->name=name;fn->type=ret;
    fn->token.kind=T_IDENT;fn->token.text=name;
    fn->params=params;fn->param_types=types;fn->param_count=decl->parameter_count;
    for(size_t p=0;p<decl->parameter_count;p++) {
        char arg[24];snprintf(arg,sizeof(arg),"arg%zu",p);
        char *arg_name=swan_arena_alloc(program,strlen(arg)+1);
        char *arg_type=swan_arena_alloc(program,strlen(decl->parameter_types[p])+1);
        if(!arg_name||!arg_type){swan_error(error,NULL,"out of memory importing C function parameters");return false;}
        strcpy(arg_name,arg);strcpy(arg_type,decl->parameter_types[p]);
        params[p]=arg_name;types[p]=arg_type;
    }
    program->functions[program->count++]=fn;
    return true;
}

static inline bool swan_import_one_c_header(Program *program,const char *header,SwanError *error) {
    size_t command_cap=strlen(header)*4+128;
    char *command=malloc(command_cap);
    if(!command){swan_error(error,NULL,"out of memory preparing C header parser");return false;}
    size_t used=(size_t)snprintf(command,command_cap,"clang -x c -std=c11 -fsyntax-only -Xclang -ast-dump ");
    command[used++]='\'';
    for(const char *p=header;*p;p++) {
        if(*p=='\'') { memcpy(command+used,"'\\''",4);used+=4; }
        else command[used++]=*p;
    }
    const char suffix[]="' 2>/dev/null";memcpy(command+used,suffix,sizeof(suffix)-1);used+=sizeof(suffix)-1;command[used]=0;
    FILE *ast=popen(command,"r");free(command);
    if(!ast){swan_error(error,NULL,"cannot start Clang while importing C header '%s'",header);return false;}
    const char *base=strrchr(header,'/');base=base?base+1:header;
    SwanCHeaderDecl decl={0};char *line=NULL;size_t line_cap=0;ssize_t length;
    bool ok=true;
    while((length=getline(&line,&line_cap,ast))>=0) {
        (void)length;
        if(strstr(line,"FunctionDecl ")) {
            if(!swan_c_header_finish_decl(program,&decl,header,error)){ok=false;break;}
            memset(&decl,0,sizeof(decl));
            char *quote=strchr(line,'\'');
            if(!quote || (!strstr(line,"<line:")&&!strstr(line,base))) continue;
            char *end=quote;while(end>line&&(end[-1]==' '||end[-1]=='\t'))end--;
            char *start=end;while(start>line&&start[-1]!=' '&&start[-1]!='\t')start--;
            if(!swan_c_header_copy(decl.name,sizeof(decl.name),start,(size_t)(end-start)))continue;
            char *open=strchr(quote+1,'(');
            if(!open || open[1]==')' || strstr(open,"..."))continue;
            char return_type[SWAN_MAX_TEXT];
            if(!swan_c_header_copy(return_type,sizeof(return_type),quote+1,(size_t)(open-(quote+1))))continue;
            decl.return_type=swan_c_header_scalar_type(return_type);
            decl.eligible=decl.return_type!=NULL;
            continue;
        }
        if(!decl.eligible)continue;
        if(strstr(line,"CompoundStmt")){decl.has_body=true;continue;}
        if(strstr(line,"ParmVarDecl ")) {
            if(decl.parameter_count>=16){decl.eligible=false;continue;}
            char *first=strchr(line,'\'');if(!first){decl.eligible=false;continue;}
            char *last=strchr(first+1,'\'');if(!last){decl.eligible=false;continue;}
            char type[SWAN_MAX_TEXT];
            if(!swan_c_header_copy(type,sizeof(type),first+1,(size_t)(last-first-1))){decl.eligible=false;continue;}
            const char *mapped=swan_c_header_scalar_type(type);
            if(!mapped || !strcmp(mapped,"void")){decl.eligible=false;continue;}
            decl.parameter_types[decl.parameter_count++]=mapped;
        }
    }
    if(ok && !swan_c_header_finish_decl(program,&decl,header,error))ok=false;
    free(line);
    int status=pclose(ast);
    if(status==-1 || !WIFEXITED(status) || WEXITSTATUS(status)!=0) {
        if(ok)swan_error(error,NULL,"Clang could not parse C header '%s'",header);
        ok=false;
    }
    return ok;
}

static inline bool swan_import_c_headers(Program *program,SwanError *error) {
    for(size_t i=0;i<program->c_header_count;i++)
        if(!swan_import_one_c_header(program,program->c_headers[i],error))return false;
    return true;
}
#endif
