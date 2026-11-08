#ifndef SWAN_COMPILER_H
#define SWAN_COMPILER_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "swan/codegen_x86_64.h"
#include "swan/sema.h"
#include <unistd.h>
#include <sys/wait.h>

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
 if(n<0||(size_t)n>=sizeof(directory)||mkdir(directory,0755)!=0&&errno!=EEXIST){swan_error(err,NULL,"cannot create lazy-module directory");return false;}
 for(size_t m=0;m<p->module_count;m++){
  if(!swan_module_is_used(p,m))continue;
  Text module_assembly={0};int previous=p->codegen_module_index;p->codegen_module_index=(int)m;
  bool generated=swan_codegen_x86_64(p,&module_assembly,err);p->codegen_module_index=previous;
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

static inline int swan_run(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--version")){puts("Swan " SWAN_VERSION);return 0;}
 if(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){printf("Swan %s\nUsage: swan [build|run] <file.swan> [--emit-asm] [--target x86_64] [--stdlib directory] [-o output]\n",SWAN_VERSION);return 0;}
 enum { SWAN_RUN_DEFAULT,SWAN_BUILD,SWAN_RUN,SWAN_EMIT } mode=SWAN_RUN_DEFAULT;
 int index=1;
 if(index<argc&&!strcmp(argv[index],"build")){mode=SWAN_BUILD;index++;}
 else if(index<argc&&!strcmp(argv[index],"run")){mode=SWAN_RUN;index++;}
 if(index>=argc){fprintf(stderr,"Swan %s\nUsage: swan [build|run] <file.swan> [--emit-asm] [--target x86_64] [--stdlib directory] [-o output]\n",SWAN_VERSION);return 2;}
 const char *input=argv[index++],*output=NULL,*stdlib_dir=getenv("SWAN_STDLIB");
 if(!stdlib_dir||!*stdlib_dir)stdlib_dir="stdlib";
 bool emit_asm=false;
 for(int i=index;i<argc;i++){
  if(!strcmp(argv[i],"--emit-asm"))emit_asm=true;
  else if(!strcmp(argv[i],"--stdlib")&&i+1<argc)stdlib_dir=argv[++i];
  else if(!strcmp(argv[i],"-o")&&i+1<argc)output=argv[++i];
  else if(!strcmp(argv[i],"--target")&&i+1<argc){
   const char *target=argv[++i];
   if(strcmp(target,"x86_64")&&strcmp(target,"x86-64")){fprintf(stderr,"swan: target '%s' is not implemented; available target: x86_64\n",target);return 2;}
  }else{fprintf(stderr,"swan: unknown option '%s'\n",argv[i]);return 2;}
 }
 if(emit_asm)mode=SWAN_EMIT;
 char *source=swan_read_file(input);if(!source){perror(input);return 1;}
 SwanError err={0};TokenStream ts;
 if(!swan_lex(source,&ts,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.items);free(ts.text_storage);free(source);return 1;}
 Program p={0};p.codegen_module_index=-1;
 if(!swan_parse(&ts,&p,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.items);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 free(ts.items);p.source_function_count=p.count;
 char *module_sources[64]={0},*module_texts[64]={0};size_t module_count=0;
 if(!swan_load_imports(&p,stdlib_dir,module_sources,module_texts,&module_count,&err)){
  fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
 }
 if(!swan_semantic_check(&p,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 swan_compute_reachability(&p);bool has_lazy=swan_has_lazy_imports(&p);
 if(mode==SWAN_EMIT&&has_lazy){fprintf(stderr,"swan: --emit-asm cannot bundle runtime-lazy modules; use 'swan build' or 'swan run'\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
 char exe[1024];bool temporary_output=output==NULL;int exe_len;
 if(output)exe_len=snprintf(exe,sizeof(exe),"%s",output);
 else if(mode==SWAN_BUILD)exe_len=snprintf(exe,sizeof(exe),"a.out");
 else exe_len=snprintf(exe,sizeof(exe),"/tmp/swan-run-%ld",(long)getpid());
 if(exe_len<0||(size_t)exe_len>=sizeof(exe)){fprintf(stderr,"swan: output path is too long\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 Text assembly={0};
 if(!swan_codegen_x86_64(&p,&assembly,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 if(has_lazy&&!swan_build_lazy_modules(&p,exe,&err)){fprintf(stderr,"swan: %s\n",err.message);swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 char runtime_source[64]={0};if(has_lazy&&!swan_write_lazy_runtime(runtime_source)){fprintf(stderr,"swan: cannot create lazy import runtime support\n");swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 if(mode==SWAN_EMIT){
  const char *dest=output?output:"a.s";FILE *of=fopen(dest,"wb");
  if(!of){perror(dest);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  bool ok=fwrite(assembly.data,1,assembly.len,of)==assembly.len;if(fclose(of)!=0)ok=false;
  if(!ok){fprintf(stderr,"swan: failed to write assembly output '%s'\n",dest);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  if(runtime_source[0])unlink(runtime_source);
  swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 0;
 }
 char temp[]="/tmp/swan-XXXXXX";int fd=mkstemp(temp);
 if(fd<0){perror("mkstemp");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 FILE *af=fdopen(fd,"wb");
 if(!af){close(fd);unlink(temp);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 bool written=fwrite(assembly.data,1,assembly.len,af)==assembly.len;
 if(fclose(af)!=0)written=false;
 if(!written){fprintf(stderr,"swan: failed to write temporary assembly\n");unlink(temp);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 if(has_lazy){
  char *cc_args[]={"cc","-rdynamic","-x","assembler",temp,"-x","c",runtime_source,"-ldl","-o",exe,NULL};
  int result=swan_exec(cc_args,true);unlink(temp);unlink(runtime_source);
  if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);if(temporary_output)swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 }else{
  char *cc_args[]={"cc","-x","assembler",temp,"-o",exe,NULL};
  int result=swan_exec(cc_args,true);unlink(temp);
  if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 }
 if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);
 if(mode==SWAN_BUILD)return 0;
 char *run_args[]={exe,NULL};int result=swan_exec(run_args,false);
 if(temporary_output){unlink(exe);if(has_lazy)swan_remove_lazy_modules(exe,&p);}
 return result<0?1:result;
}
#endif
