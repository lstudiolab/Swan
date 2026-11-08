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
static inline void swan_release_modules(char **sources,char **texts,size_t count){for(size_t i=0;i<count;i++){free(sources[i]);free(texts[i]);}}
static inline bool swan_load_imports(Program *p,const char *stdlib_dir,char **sources,char **texts,size_t *module_count,SwanError *err){
 const char *loaded[64];size_t loaded_count=0,next=0;
 while(next<p->import_count){
  const char *name=p->imports[next++];bool seen=false;
  for(size_t i=0;i<loaded_count;i++)if(!strcmp(loaded[i],name)){seen=true;break;}
  if(seen)continue;
  if(loaded_count>=64||*module_count>=64){swan_error(err,NULL,"standard-library import limit exceeded");return false;}
  loaded[loaded_count++]=name;
  char path[2048];int path_len=snprintf(path,sizeof(path),"%s/%s.swan",stdlib_dir,name);
  if(path_len<0||(size_t)path_len>=sizeof(path)){swan_error(err,NULL,"standard-library path is too long");return false;}
  char *source=swan_read_file(path);
  if(!source){swan_error(err,NULL,"cannot load standard-library module '%s' from '%s'",name,path);return false;}
  TokenStream module_ts;
  if(!swan_lex(source,&module_ts,err)){free(source);free(module_ts.items);free(module_ts.text_storage);return false;}
  size_t slot=(*module_count)++;sources[slot]=source;texts[slot]=module_ts.text_storage;free(module_ts.items);
  if(!swan_parse(&module_ts,p,err)){char detail[sizeof(err->message)];snprintf(detail,sizeof(detail),"%s",err->message);swan_error(err,NULL,"in module '%s': %s",name,detail);return false;}
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
static inline int swan_run(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--version")){puts("Swan " SWAN_VERSION);return 0;}
 if(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){printf("Swan %s\nUsage: swan [build|run] <file.swan> [--emit-asm] [--target x86_64] [-o output]\n",SWAN_VERSION);return 0;}
 enum { SWAN_RUN_DEFAULT,SWAN_BUILD,SWAN_RUN,SWAN_EMIT } mode=SWAN_RUN_DEFAULT;
 int index=1;
 if(index<argc&&!strcmp(argv[index],"build")){mode=SWAN_BUILD;index++;}
 else if(index<argc&&!strcmp(argv[index],"run")){mode=SWAN_RUN;index++;}
 if(index>=argc){fprintf(stderr,"Swan %s\nUsage: swan [build|run] <file.swan> [--emit-asm] [--target x86_64] [-o output]\n",SWAN_VERSION);return 2;}
 const char *input=argv[index++],*output=NULL;
 bool emit_asm=false;
 for(int i=index;i<argc;i++){
  if(!strcmp(argv[i],"--emit-asm"))emit_asm=true;
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
 Program p={0};
 if(!swan_parse(&ts,&p,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.items);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 free(ts.items);
 if(!swan_semantic_check(&p,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 Text assembly={0};
 if(!swan_codegen_x86_64(&p,&assembly,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 if(mode==SWAN_EMIT){
  const char *dest=output?output:"a.s";FILE *of=fopen(dest,"wb");
  if(!of){perror(dest);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  bool ok=fwrite(assembly.data,1,assembly.len,of)==assembly.len&&fclose(of)==0;
  if(!ok){fprintf(stderr,"swan: failed to write assembly output '%s'\n",dest);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 0;
 }
 char temp[]="/tmp/swan-XXXXXX";int fd=mkstemp(temp);
 if(fd<0){perror("mkstemp");free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 FILE *af=fdopen(fd,"wb");
 if(!af){close(fd);unlink(temp);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 bool written=fwrite(assembly.data,1,assembly.len,af)==assembly.len;
 if(fclose(af)!=0)written=false;
 if(!written){fprintf(stderr,"swan: failed to write temporary assembly\n");unlink(temp);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 char exe[1024];
 bool temporary_output=output==NULL;
 if(output)snprintf(exe,sizeof(exe),"%s",output);
 else if(mode==SWAN_BUILD)snprintf(exe,sizeof(exe),"a.out");
 else snprintf(exe,sizeof(exe),"/tmp/swan-run-%ld",(long)getpid());
 char *cc_args[]={"cc","-x","assembler",temp,"-o",exe,NULL};
 int result=swan_exec(cc_args,true);unlink(temp);
 if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);
 if(mode==SWAN_BUILD)return 0;
 char *run_args[]={exe,NULL};result=swan_exec(run_args,false);
 if(temporary_output)unlink(exe);
 return result<0?1:result;
}
#endif
