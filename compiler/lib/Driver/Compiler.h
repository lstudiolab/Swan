#ifndef SWAN_COMPILER_H
#define SWAN_COMPILER_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "swan/codegen.h"
#include "swan/target.h"
#include "swan/ir.h"
#include "swan/sema.h"
#include "swan/c_header.h"
#include "swan/irgen.h"
#include "swan/optimization.h"
#include "swan/codegen_wasm32.h"
#include "swan/codegen_aarch64.h"
#include "swan/codegen_riscv64.h"
#include "swan/codegen_armv7.h"
#include "swan/codegen_llvm.h"
#include "swan/emitter.h"
#include "swan/linker.h"
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <spawn.h>
extern char **environ;

static inline char *swan_read_file(const char *path){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
 if(n<0){fclose(f);return NULL;}rewind(f);
 char *s=malloc((size_t)n+1);if(!s){fclose(f);return NULL;}
 size_t got=fread(s,1,(size_t)n,f);bool failed=ferror(f)!=0;fclose(f);
 if(failed||memchr(s,0,got)){free(s);errno=EILSEQ;return NULL;}s[got]=0;return s;
}
static inline void swan_release_modules(char **sources,char **texts,size_t count){
 (void)count;
 /* Source buffers and token text are owned by Program's source manager. */
 free(sources);free(texts);
}
static inline char *swan_resolve_module(const char *name,const char *const *roots,size_t root_count,char resolved[2048]){
 for(size_t i=0;i<root_count;i++){
  int n=snprintf(resolved,2048,"%s/%s.swan",roots[i],name);
  if(n<0||(size_t)n>=2048)continue;
  char *source=swan_read_file(resolved);if(source)return source;
 }
 resolved[0]=0;return NULL;
}
static inline bool swan_load_imports(Program *p,const char *const *module_paths,size_t module_path_count,char ***sources_io,char ***texts_io,size_t *module_count,size_t *source_capacity,size_t *text_capacity,SwanError *err){
 size_t next=0;
 while(next<p->import_count){
  const char *name=p->imports[next++];bool seen=false;
  for(size_t i=0;i<p->module_count;i++)if(!strcmp(p->module_names[i],name)){seen=true;break;}
  if(seen)continue;
  if(p->module_count>(size_t)INT_MAX){swan_error(err,NULL,"module index exceeds compiler representation");return false;}
  size_t module=0;if(!swan_program_add_module(p,name,p->count,&module)){swan_error(err,NULL,"out of memory recording imported module");return false;}
  char path[2048];char *source=swan_resolve_module(name,module_paths,module_path_count,path);
  if(!source){swan_error(err,NULL,"cannot resolve module '%s' in %zu module search path(s)",name,module_path_count);return false;}
  TokenStream module_ts={0};
  if(!swan_lex(source,&module_ts,err)){free(source);free(module_ts.items);free(module_ts.text_storage);return false;}
  size_t needed=*module_count+1;
  if(needed<*module_count||!swan_reserve_array((void**)sources_io,source_capacity,needed,sizeof(**sources_io))||
     !swan_reserve_array((void**)texts_io,text_capacity,needed,sizeof(**texts_io))){
   free(source);free(module_ts.items);free(module_ts.text_storage);swan_error(err,NULL,"out of memory retaining imported module sources");return false;
  }
  if(!swan_program_add_source(p,path,source,module_ts.text_storage)){
   free(source);free(module_ts.items);free(module_ts.text_storage);
   swan_error(err,NULL,"out of memory retaining imported source file '%s'",path);return false;
  }
  size_t slot=(*module_count)++;(*sources_io)[slot]=source;(*texts_io)[slot]=module_ts.text_storage;
  source=NULL;module_ts.text_storage=NULL;p->current_source_index=p->source_count-1;
  swan_token_stream_set_source_name(&module_ts,p->source_files[p->current_source_index].path);
  bool parsed=swan_parse(&module_ts,p,err);free(module_ts.items);
  if(!parsed){
   char detail[sizeof(err->message)];unsigned line=err?err->line:0,col=err?err->col:0;
   if(err)snprintf(detail,sizeof(detail),"%s",err->message);else snprintf(detail,sizeof(detail),"module parsing failed");
   swan_error(err,NULL,"in module '%s': %s",name,detail);if(err){err->line=line;err->col=col;}return false;
  }
  p->module_ends[module]=p->count;
  for(size_t i=p->module_starts[module];i<p->module_ends[module];i++){
   Stmt *function=p->functions[i];function->imported=true;function->module_index=(int)module;
   size_t name_length=strlen(function->name);
   if(name_length>SIZE_MAX-64){swan_error(err,&function->token,"module function linker name is too long");return false;}
   char *link_name=swan_arena_alloc(p,name_length+64);
   if(!link_name){swan_error(err,&function->token,"out of memory creating module-qualified function identity");return false;}
   snprintf(link_name,name_length+64,"__swan_module_%zu_%s",module,function->name);
   if(!function->external)function->link_name=link_name;
  }
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
 if(!args||!args[0]){errno=EINVAL;return -1;}
 pid_t pid=0;
 int spawn_error=search_path
     ?posix_spawnp(&pid,args[0],NULL,NULL,args,environ)
     :posix_spawn(&pid,args[0],NULL,NULL,args,environ);
 if(spawn_error){errno=spawn_error;return -1;}
 return swan_wait_child(pid);
}

static inline size_t swan_function_index(Program *p,Stmt *fn){for(size_t i=0;i<p->count;i++)if(p->functions[i]==fn)return i;return SIZE_MAX;}
static inline void swan_mark_block(Program *p,Stmt **body,size_t count);
static inline void swan_mark_expr(Program *p,Expr *e){
 if(!e)return;
 if(e->kind==E_CALL){
  const char *callee=swan_expr_terminal_name(e->left);
  if(callee){
   Stmt *fn=swan_find_link_function(p,callee);
   if(fn){
    size_t i=swan_function_index(p,fn);
    if(i!=SIZE_MAX&&!p->function_reachable[i]){
     p->function_reachable[i]=true;
     swan_mark_block(p,fn->body,fn->body_count);
    }
   }
  }
  for(size_t i=0;i<e->argc;i++)swan_mark_expr(p,e->args[i]);
  return;
 }
 swan_mark_expr(p,e->left);swan_mark_expr(p,e->right);
 for(size_t i=0;i<e->argc;i++)swan_mark_expr(p,e->args[i]);
}
static inline void swan_mark_block(Program *p,Stmt **body,size_t count){
 for(size_t i=0;i<count;i++){Stmt *s=body[i];swan_mark_expr(p,s->value);swan_mark_expr(p,s->condition);swan_mark_block(p,s->body,s->body_count);swan_mark_block(p,s->alternate,s->alt_count);}
}
static inline bool swan_compute_reachability(Program *p,SwanError *err){
 free(p->function_reachable);p->function_reachable=p->count?calloc(p->count,sizeof(*p->function_reachable)):NULL;
 if(p->count&&!p->function_reachable){swan_error(err,NULL,"out of memory computing function reachability");return false;}
 p->reachability_computed=true;
 Stmt *main_fn=swan_find_function(p,"main");size_t index=swan_function_index(p,main_fn);
 if(index!=SIZE_MAX){p->function_reachable[index]=true;swan_mark_block(p,main_fn->body,main_fn->body_count);}return true;
}
static inline bool swan_module_is_used(Program *p,size_t module){
 for(size_t i=p->module_starts[module];i<p->module_ends[module];i++)if(p->function_reachable[i])return true;
 return false;
}
static inline bool swan_has_lazy_imports(Program *p){
 if(p->bundle_imports)return false;
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
 " const char *root=*exe?exe:\"/\";size_t rn=strlen(root),bn=strlen(base),mn=strlen(module);\n"
 " if(rn+1+bn+9+mn+3>=sizeof(path)){fprintf(stderr,\"swan: module path too long\\\\n\");abort();}\n"
 " char *cursor=path;memcpy(cursor,root,rn);cursor+=rn;*cursor++='/';memcpy(cursor,base,bn);cursor+=bn;memcpy(cursor,\".modules/\",9);cursor+=9;memcpy(cursor,module,mn);cursor+=mn;memcpy(cursor,\".so\",3);cursor[3]=0;\n"
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
