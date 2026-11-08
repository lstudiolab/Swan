#ifndef SWAN_COMPILER_H
#define SWAN_COMPILER_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "swan/codegen.h"
#include "swan/target.h"
#include "swan/ir.h"
#include "swan/sema.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include "swan/svm.h"
#include "swan/codegen_wasm32.h"
#include "swan/emitter.h"
#include "swan/linker.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

static inline char *swan_read_file(const char *path){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
 if(n<0){fclose(f);return NULL;}rewind(f);
 char *s=malloc((size_t)n+1);if(!s){fclose(f);return NULL;}
 size_t got=fread(s,1,(size_t)n,f);bool failed=ferror(f)!=0;fclose(f);
 if(failed){free(s);return NULL;}s[got]=0;return s;
}
static inline void swan_release_modules(char **sources,char **texts,size_t count){
 for(size_t i=0;i<count;i++){free(sources[i]);free(texts[i]);}
}
static inline bool swan_load_imports(Program *p,const char *stdlib_dir,char **sources,char **texts,size_t *module_count,SwanError *err){
 const char *loaded[64];size_t loaded_count=0,next=0;
 while(next<p->import_count){
  const char *name=p->imports[next++];bool seen=false;
  for(size_t i=0;i<loaded_count;i++)if(!strcmp(loaded[i],name)){seen=true;break;}
  if(seen)continue;
  if(loaded_count>=64||*module_count>=64){swan_error(err,NULL,"standard-library import limit exceeded");return false;}
  loaded[loaded_count++]=name;
  if(p->module_count>=64){swan_error(err,NULL,"too many imported modules");return false;}
  size_t module=(size_t)p->module_count++;
  p->module_names[module]=name;p->module_starts[module]=p->count;
  char path[2048];int path_len=snprintf(path,sizeof(path),"%s/%s.swan",stdlib_dir,name);
  if(path_len<0||(size_t)path_len>=sizeof(path)){swan_error(err,NULL,"standard-library path is too long");return false;}
  char *source=swan_read_file(path);
  if(!source){swan_error(err,NULL,"cannot load standard-library module '%s' from '%s'",name,path);return false;}
  TokenStream module_ts;
  if(!swan_lex(source,&module_ts,err)){free(source);free(module_ts.items);free(module_ts.text_storage);return false;}
  size_t slot=(*module_count)++;sources[slot]=source;texts[slot]=module_ts.text_storage;free(module_ts.items);
  if(!swan_parse(&module_ts,p,err)){
   char detail[sizeof(err->message)];snprintf(detail,sizeof(detail),"%s",err->message);
   swan_error(err,NULL,"in module '%s': %s",name,detail);return false;
  }
  p->module_ends[module]=p->count;
  for(size_t i=p->module_starts[module];i<p->module_ends[module];i++){p->functions[i]->imported=true;p->functions[i]->module_index=(int)module;}
 }
 return true;
}
static inline int swan_wait_child(pid_t pid){
 int status=0;while(waitpid(pid,&status,0)<0){if(errno==EINTR)continue;return -1;}
 if(WIFEXITED(status))return WEXITSTATUS(status);
 if(WIFSIGNALED(status))return 128+WTERMSIG(status);
 return 1;
}
static inline int swan_exec(char *const args[],bool search_path){
 pid_t pid=fork();if(pid<0)return -1;
 if(pid==0){if(search_path)execvp(args[0],args);else execv(args[0],args);perror(args[0]);_exit(127);}
 return swan_wait_child(pid);
}

static inline size_t swan_function_index(Program *p,Stmt *fn){for(size_t i=0;i<p->count;i++)if(p->functions[i]==fn)return i;return SIZE_MAX;}
static inline void swan_mark_block(Program *p,Stmt **body,size_t count);
static inline void swan_mark_expr(Program *p,Expr *e){
 if(!e)return;
 if(e->kind==E_CALL&&e->left&&e->left->kind==E_NAME){
  Stmt *fn=swan_find_function(p,e->left->token.text);
  if(fn){size_t i=swan_function_index(p,fn);if(i!=SIZE_MAX&&!p->function_reachable[i]){p->function_reachable[i]=true;swan_mark_block(p,fn->body,fn->body_count);}}
  for(size_t i=0;i<e->argc;i++)swan_mark_expr(p,e->args[i]);
  return;
 }
 swan_mark_expr(p,e->left);swan_mark_expr(p,e->right);
 for(size_t i=0;i<e->argc;i++)swan_mark_expr(p,e->args[i]);
}
static inline void swan_mark_block(Program *p,Stmt **body,size_t count){
 for(size_t i=0;i<count;i++){Stmt *s=body[i];swan_mark_expr(p,s->value);swan_mark_expr(p,s->condition);swan_mark_block(p,s->body,s->body_count);swan_mark_block(p,s->alternate,s->alt_count);}
}
static inline void swan_compute_reachability(Program *p){
 memset(p->function_reachable,0,sizeof(p->function_reachable));
 Stmt *main_fn=swan_find_function(p,"main");size_t index=swan_function_index(p,main_fn);
 if(index!=SIZE_MAX){p->function_reachable[index]=true;swan_mark_block(p,main_fn->body,main_fn->body_count);}
}
static inline bool swan_module_is_used(Program *p,size_t module){
 for(size_t i=p->module_starts[module];i<p->module_ends[module];i++)if(p->function_reachable[i])return true;
 return false;
}
static inline bool swan_has_lazy_imports(Program *p){
 for(size_t i=0;i<p->module_count;i++)if(swan_module_is_used(p,i))return true;
 return false;
}
static inline bool swan_write_temp(const char *pattern,const char *data,size_t length,char path[64]){
 snprintf(path,64,"%s",pattern);int fd=mkstemp(path);if(fd<0)return false;
 size_t done=0;while(done<length){ssize_t n=write(fd,data+done,length-done);if(n<0){if(errno==EINTR)continue;close(fd);unlink(path);return false;}done+=(size_t)n;}
 if(close(fd)!=0){unlink(path);return false;}return true;
}
static inline bool swan_build_lazy_modules(Program *p,const char *exe,SwanError *err){
 if(!swan_has_lazy_imports(p))return true;
 char directory[1200];int n=snprintf(directory,sizeof(directory),"%s.modules",exe);
 if(n<0||(size_t)n>=sizeof(directory)||(mkdir(directory,0755)!=0&&errno!=EEXIST)){swan_error(err,NULL,"cannot create lazy-module directory");return false;}
 for(size_t m=0;m<p->module_count;m++){
  if(!swan_module_is_used(p,m))continue;
  Text module_assembly={0};int previous=p->codegen_module_index;p->codegen_module_index=(int)m;
  bool generated=swan_codegen(p,&module_assembly,swan_target_find("x86_64"),err);p->codegen_module_index=previous;
  if(!generated){free(module_assembly.data);return false;}
  char temp[64];if(!swan_write_temp("/tmp/swan-module-XXXXXX",module_assembly.data,module_assembly.len,temp)){free(module_assembly.data);swan_error(err,NULL,"cannot write temporary module assembly");return false;}
  free(module_assembly.data);
  char output[1400];n=snprintf(output,sizeof(output),"%s/%s.so",directory,p->module_names[m]);
  if(n<0||(size_t)n>=sizeof(output)){unlink(temp);swan_error(err,NULL,"lazy-module output path is too long");return false;}
  char *args[]={"cc","-shared","-fPIC","-x","assembler",temp,"-o",output,NULL};
  int result=swan_exec(args,true);unlink(temp);
  if(result!=0){swan_error(err,NULL,"failed to compile lazy standard-library module '%s'",p->module_names[m]);return false;}
 }
 return true;
}
static inline bool swan_write_lazy_runtime(char path[64]){
 static const char source[]=
 "#define _POSIX_C_SOURCE 200809L\n"
 "#include <dlfcn.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <unistd.h>\n"
 "void *swan_resolve(const char *module,const char *symbol){\n"
 " static struct {char name[128];void *handle;} cache[64];\n"
 " static size_t count=0;void *handle=NULL;\n"
 " for(size_t i=0;i<count;i++)if(!strcmp(cache[i].name,module)){handle=cache[i].handle;break;}\n"
 " if(!handle){char exe[4096],path[8192];ssize_t n=readlink(\"/proc/self/exe\",exe,sizeof(exe)-1);if(n<0){perror(\"swan: locate executable\");abort();}exe[n]=0;\n"
 " char *base=strrchr(exe,'/');if(base){*base=0;base++;}else base=exe;\n"
 " snprintf(path,sizeof(path),\"%s/%s.modules/%s.so\",*exe?exe:\"/\",base,module);\n"
 " handle=dlopen(path,RTLD_NOW|RTLD_GLOBAL);if(!handle){fprintf(stderr,\"swan: cannot load module %s: %s\\\\n\",module,dlerror());abort();}\n"
 " if(count<64){snprintf(cache[count].name,sizeof(cache[count].name),\"%s\",module);cache[count++].handle=handle;}}\n"
 " dlerror();void *function=dlsym(handle,symbol);const char *error=dlerror();if(error){fprintf(stderr,\"swan: cannot resolve %s: %s\\\\n\",symbol,error);abort();}return function;\n"
 "}\n";
 return swan_write_temp("/tmp/swan-runtime-XXXXXX",source,sizeof(source)-1,path);
}
static inline void swan_remove_lazy_modules(const char *exe,Program *p){
 char directory[1200];int n=snprintf(directory,sizeof(directory),"%s.modules",exe);if(n<0||(size_t)n>=sizeof(directory))return;
 for(size_t i=0;i<p->module_count;i++){char path[1400];n=snprintf(path,sizeof(path),"%s/%s.so",directory,p->module_names[i]);if(n>0&&(size_t)n<sizeof(path))unlink(path);}
 rmdir(directory);
}


#endif
