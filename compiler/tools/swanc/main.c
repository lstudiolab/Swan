#define _POSIX_C_SOURCE 200809L
#include "swan/compiler.h"
#include <glob.h>

/* The executable entry point owns command-line orchestration. */
typedef enum {
 SWAN_RUN_DEFAULT, SWAN_BUILD, SWAN_RUN, SWAN_EMIT
} SwanDriverMode;
static bool swan_has_suffix(const char *path,const char *suffix){
 size_t a=strlen(path),b=strlen(suffix);return a>=b&&!strcmp(path+a-b,suffix);
}
static void swan_print_diagnostic(const char *prefix,const SwanError *error){
 if(!error)return;
 if(error->source_name&&error->line)
  fprintf(stderr,"%s:%s:%u:%u: %s\n",prefix,error->source_name,error->line,error->col,error->message);
 else if(error->line)
  fprintf(stderr,"%s:%u:%u: %s\n",prefix,error->line,error->col,error->message);
 else
  fprintf(stderr,"%s: %s\n",prefix,error->message);
}
static bool swan_append_input(const char ***items,size_t *count,size_t *capacity,const char *path){
 if(!items||!count||!capacity||!path||*count==SIZE_MAX||
    !swan_reserve_array((void**)items,capacity,*count+1,sizeof(**items)))return false;
 (*items)[(*count)++]=path;return true;
}
static bool swan_collect_directory(const char *directory,const char ***swan,size_t *sn,size_t *sc,
                                   const char ***c,size_t *cn,size_t *cc){
 char pattern[4096];glob_t matches={0};int n=snprintf(pattern,sizeof(pattern),"%s/*.swan",directory);
 if(n<0||(size_t)n>=sizeof(pattern))return false;
 int result=glob(pattern,0,NULL,&matches);
 if(result!=0&&result!=GLOB_NOMATCH){globfree(&matches);return false;}
 for(size_t i=0;i<matches.gl_pathc;i++){
  char *path=strdup(matches.gl_pathv[i]);
  if(!path||!swan_append_input(swan,sn,sc,path)){free(path);globfree(&matches);return false;}
 }
 globfree(&matches);memset(&matches,0,sizeof(matches));
 n=snprintf(pattern,sizeof(pattern),"%s/*.c",directory);
 if(n<0||(size_t)n>=sizeof(pattern))return false;
 result=glob(pattern,0,NULL,&matches);
 if(result!=0&&result!=GLOB_NOMATCH){globfree(&matches);return false;}
 for(size_t i=0;i<matches.gl_pathc;i++){
  char *path=strdup(matches.gl_pathv[i]);
  if(!path||!swan_append_input(c,cn,cc,path)){free(path);globfree(&matches);return false;}
 }
 globfree(&matches);return true;
}
static bool swan_append_c_assembly(FILE *output,const char *const *sources,size_t count){
 for(size_t i=0;i<count;i++){
  char temp[]="/tmp/swanc-c-XXXXXX";int fd=mkstemp(temp);if(fd<0)return false;
  if(close(fd)!=0){unlink(temp);return false;}
  char *args[]={"cc","-S","-masm=intel","-o",temp,(char*)sources[i],NULL};
  int result=swan_exec(args,true);if(result!=0){unlink(temp);return false;}
  FILE *input=fopen(temp,"rb");if(!input){unlink(temp);return false;}
  char buffer[16384];size_t n;bool ok=true;
  while((n=fread(buffer,1,sizeof(buffer),input))>0)if(fwrite(buffer,1,n,output)!=n){ok=false;break;}
  if(ferror(input))ok=false;
  if(fclose(input)!=0)ok=false;
  unlink(temp);if(!ok)return false;
  if(fputc('\n',output)==EOF)return false;
 }
 return true;
}
static bool swan_make_output_dir(const char *directory){
 struct stat info;
 if(mkdir(directory,0755)==0)return true;
 return errno==EEXIST&&stat(directory,&info)==0&&S_ISDIR(info.st_mode);
}
static const char *swan_source_basename(const char *path){
 const char *base=strrchr(path,'/');
 return base?base+1:path;
}
static bool swan_source_output_path(char *out,size_t capacity,const char *directory,
                                    const char *source_path,const char *extension){
 const char *base=swan_source_basename(source_path);
 size_t length=strlen(base);
 if(length>=5&&!strcmp(base+length-5,".swan"))length-=5;
 if(length>(size_t)INT_MAX)return false;
 int n=snprintf(out,capacity,"%s/%.*s%s",directory,(int)length,base,extension);
 if(n<0||(size_t)n>=capacity)return false;
 return true;
}
static bool swan_emit_project_sources(Program *program,const char *directory,
                                      const SwanTarget *target,int mode,
                                      const char *const *c_inputs,size_t c_count,
                                      SwanError *error){
 (void)mode;
 if(!swan_make_output_dir(directory))return false;
 const char *extension=target->architecture==SWAN_ARCH_WASM64?".wasm":".s";
 program->codegen_source_index=-1;
 program->codegen_include_external_functions=target->architecture!=SWAN_ARCH_WASM64;
 program->codegen_force_ast=target->architecture==SWAN_ARCH_X86_64&&
                            (program->no_heap||swan_program_requires_ast_compatibility(program));
 for(size_t i=0;i<program->source_count;i++){
  if(i>(size_t)INT_MAX){swan_error(error,NULL,"source index exceeds code-generation representation");goto fail;}
  char path[4096];
  if(!swan_source_output_path(path,sizeof(path),directory,
                              program->source_files[i].path,extension)){
   swan_error(error,NULL,"source output path is too long for '%s'",program->source_files[i].path);goto fail;
  }
  for(size_t prior=0;prior<i;prior++){
   char previous[4096];
   if(!swan_source_output_path(previous,sizeof(previous),directory,
                               program->source_files[prior].path,extension))goto fail;
   if(!strcmp(path,previous)){
    char unique[4096];
    const char *base=swan_source_basename(program->source_files[i].path);
    size_t length=strlen(base);if(length>=5&&!strcmp(base+length-5,".swan"))length-=5;
    if(length>(size_t)INT_MAX)goto fail;
    int n=snprintf(unique,sizeof(unique),"%s/%.*s_%zu%s",directory,(int)length,base,i,extension);
    if(n<0||(size_t)n>=sizeof(unique))goto fail;
    snprintf(path,sizeof(path),"%s",unique);break;
   }
  }
  program->codegen_source_index=(int)i;
  SwanSVMOutput generated={0};
  if(!swan_svm_codegen(program,target,&generated,error))goto fail;
  bool ok=false;
  if(generated.kind==SWAN_SVM_OUTPUT_WASM){
   FILE *file=fopen(path,"wb");
   ok=file&&fwrite(generated.data,1,generated.length,file)==generated.length;
   if(file&&fclose(file)!=0)ok=false;
  }else{
   Text assembly={.data=generated.data,.len=generated.length};
   FILE *file=fopen(path,"wb");
   ok=file&&swan_emit_assembly(file,&assembly);
   if(file&&fclose(file)!=0)ok=false;
  }
  free(generated.data);
  if(!ok){swan_error(error,NULL,"cannot write translation unit output '%s'",path);goto fail;}
 }
 program->codegen_source_index=-1;program->codegen_force_ast=false;program->codegen_include_external_functions=false;
 for(size_t i=0;i<c_count;i++){
  char path[4096];
  int n=snprintf(path,sizeof(path),"%s/native_%zu.s",directory,i);
  if(n<0||(size_t)n>=sizeof(path))goto fail;
  FILE *file=fopen(path,"wb");
  bool ok=file&&swan_append_c_assembly(file,&c_inputs[i],1);
  if(file&&fclose(file)!=0)ok=false;
  if(!ok){swan_error(error,NULL,"cannot write C source assembly '%s'",path);goto fail;}
 }
 return true;
fail:
 program->codegen_source_index=-1;program->codegen_force_ast=false;program->codegen_include_external_functions=false;
 return false;
}
static bool swan_compile_project_objects(Program *program,char ***objects_out,
                                         size_t *object_count_out,SwanError *error){
 if(!program||!objects_out||!object_count_out)return false;
 *objects_out=NULL;*object_count_out=0;
 program->codegen_source_index=-1;
 program->codegen_include_external_functions=true;
 program->codegen_force_ast=program->no_heap||swan_program_requires_ast_compatibility(program);
 if(program->source_count>SIZE_MAX/sizeof(char*)){
  swan_error(error,NULL,"project object list size overflow");return false;
 }
 char **objects=calloc(program->source_count,sizeof(*objects));
 if(program->source_count&&!objects){swan_error(error,NULL,"out of memory creating project object list");return false;}
 for(size_t i=0;i<program->source_count;i++){
  if(i>(size_t)INT_MAX){swan_error(error,NULL,"source index exceeds code-generation representation");goto fail;}
  char assembly_path[]="/tmp/swan-tu-XXXXXX";
  int assembly_fd=mkstemp(assembly_path);
  if(assembly_fd<0){swan_error(error,NULL,"cannot create temporary translation-unit assembly");goto fail;}
  program->codegen_source_index=(int)i;
  SwanSVMOutput generated={0};
  if(!swan_svm_codegen(program,swan_target_find("x86_64"),&generated,error)){
   program->codegen_source_index=-1;close(assembly_fd);unlink(assembly_path);goto fail;
  }
  program->codegen_source_index=-1;
  FILE *assembly_file=fdopen(assembly_fd,"wb");
  bool written=assembly_file&&swan_emit_assembly(assembly_file,
      &(Text){.data=generated.data,.len=generated.length});
  if(assembly_file&&fclose(assembly_file)!=0)written=false;
  else if(!assembly_file)close(assembly_fd);
  free(generated.data);
  if(!written){unlink(assembly_path);swan_error(error,NULL,"cannot write temporary assembly for '%s'",program->source_files[i].path);goto fail;}
  char object_path[]="/tmp/swan-obj-XXXXXX";
  int object_fd=mkstemp(object_path);
  if(object_fd<0){unlink(assembly_path);swan_error(error,NULL,"cannot create temporary project object");goto fail;}
  close(object_fd);
  char *args[]={"cc","-c","-x","assembler",assembly_path,"-o",object_path,NULL};
  int result=swan_exec(args,true);unlink(assembly_path);
  if(result!=0){unlink(object_path);swan_error(error,NULL,"assembler failed for source file '%s'",program->source_files[i].path);goto fail;}
  objects[i]=malloc(sizeof(object_path));
  if(!objects[i]){unlink(object_path);swan_error(error,NULL,"out of memory retaining project object path");goto fail;}
  memcpy(objects[i],object_path,sizeof(object_path));
  (*object_count_out)++;
 }
 *objects_out=objects;program->codegen_source_index=-1;program->codegen_force_ast=false;program->codegen_include_external_functions=false;return true;
fail:
 for(size_t i=0;i<program->source_count;i++){if(objects[i]){unlink(objects[i]);free(objects[i]);}}
 free(objects);return false;
}
static int swan_link_project_objects(char *const *objects,size_t object_count,
                                     const char *const *c_inputs,size_t c_count,
                                     const char *const *link_args,size_t link_count,
                                     const char *output){
 if(object_count>SIZE_MAX-c_count||object_count+c_count>SIZE_MAX-link_count-4||
    object_count+c_count+link_count+4>SIZE_MAX/sizeof(char*))return 2;
 size_t capacity=object_count+c_count+link_count+4;
 char **args=calloc(capacity,sizeof(*args));if(!args)return 2;
 size_t count=0;args[count++]="cc";
 for(size_t i=0;i<object_count;i++)args[count++]=objects[i];
 for(size_t i=0;i<c_count;i++)args[count++]=(char*)c_inputs[i];
 for(size_t i=0;i<link_count;i++)args[count++]=(char*)link_args[i];
 args[count++]="-o";args[count++]=(char*)output;args[count]=NULL;
 int result=swan_exec(args,true);free(args);return result;
}
static void swan_release_cli_inputs(const char **swan_inputs,size_t swan_count,
                                      const char **c_inputs,size_t c_count,
                                      const char **link_args,bool owned_paths){
 if(owned_paths){
  for(size_t i=0;i<swan_count;i++)free((void*)swan_inputs[i]);
  for(size_t i=0;i<c_count;i++)free((void*)c_inputs[i]);
 }
 free((void*)swan_inputs);free((void*)c_inputs);free((void*)link_args);
}
static int swan_run(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--version")){puts("Swan " SWAN_VERSION);return 0;}
 if(argc==2&&!strcmp(argv[1],"--list-targets")){swan_target_list(stdout);return 0;}
 if(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){printf("Swan %s\nUsage: swanc [build|run] <file.swan|directory> [more.swan ...] [native.c ...] [--low-resource | --jobs 1] [--target x86_64|aarch64|wasm64] [--module-path directory ...] [--lazy-imports] [--no-heap] [--warnings-as-errors] [--link object] [-L dir] [-l library] [-o output]\n  Multi-file --emit-asm writes one target artifact per Swan source into the -o directory.\n",SWAN_VERSION);return 0;}
 SwanDriverMode mode=SWAN_RUN_DEFAULT;
 int index=1;
 if(index<argc&&!strcmp(argv[index],"build")){mode=SWAN_BUILD;index++;}
 else if(index<argc&&!strcmp(argv[index],"run")){mode=SWAN_RUN;index++;}
 if(index<argc&&!strcmp(argv[index],"--profile")){swan_profile_enabled=true;index++;}
 if(index>=argc){fprintf(stderr,"Swan %s\nUsage: swanc [build|run] <file.swan|directory> [more.swan ...] [--low-resource | --jobs 1] [--emit-asm] [--target x86_64|aarch64|wasm64] [--stdlib directory] [--module-path directory ...] [-o output]\n",SWAN_VERSION);return 2;}
 const char **swan_inputs=NULL,**c_inputs=NULL,**link_args=NULL;
 size_t swan_input_count=0,swan_input_capacity=0,c_input_count=0,c_input_capacity=0;
 size_t link_arg_count=0,link_arg_capacity=0;
 bool directory_inputs_owned=false;
 #define SWAN_RETURN(value) do { \
  swan_release_cli_inputs(swan_inputs,swan_input_count,c_inputs,c_input_count,link_args,directory_inputs_owned); \
  return (value); \
 } while(0)
 const char *first=argv[index++];struct stat first_stat;
 if(stat(first,&first_stat)==0&&S_ISDIR(first_stat.st_mode)){
  directory_inputs_owned=true;
  if(!swan_collect_directory(first,&swan_inputs,&swan_input_count,&swan_input_capacity,&c_inputs,&c_input_count,&c_input_capacity)){
   fprintf(stderr,"swanc: cannot collect sources from '%s'\n",first);SWAN_RETURN(2);
  }
 }else if(swan_has_suffix(first,".swan")){if(!swan_append_input(&swan_inputs,&swan_input_count,&swan_input_capacity,first)){fprintf(stderr,"swanc: out of memory recording source input\\n");SWAN_RETURN(2);}}
 else if(swan_has_suffix(first,".c")){if(!swan_append_input(&c_inputs,&c_input_count,&c_input_capacity,first)){fprintf(stderr,"swanc: out of memory recording C input\\n");SWAN_RETURN(2);}}
 else {fprintf(stderr,"swanc: expected a .swan file or source directory\n");SWAN_RETURN(2);}
 while(index<argc&&(swan_has_suffix(argv[index],".swan")||swan_has_suffix(argv[index],".c"))){
  if(swan_has_suffix(argv[index],".swan")){
   if(!swan_append_input(&swan_inputs,&swan_input_count,&swan_input_capacity,argv[index++])){fprintf(stderr,"swanc: out of memory recording source input\\n");SWAN_RETURN(2);}
  }else{
   if(!swan_append_input(&c_inputs,&c_input_count,&c_input_capacity,argv[index++])){fprintf(stderr,"swanc: out of memory recording C input\\n");SWAN_RETURN(2);}
  }
 }
 if(!swan_input_count){fprintf(stderr,"swanc: source directory contains no .swan files\n");SWAN_RETURN(2);}
 const char *output=NULL,*stdlib_dir=getenv("SWAN_STDLIB");
 if(!stdlib_dir||!*stdlib_dir)stdlib_dir="stdlib";
 const char *module_paths[64];size_t module_path_count=1;module_paths[0]=stdlib_dir;
 bool emit_asm=false,lazy_imports=false,warnings_as_errors=false,no_heap=false;size_t compile_jobs=1;
 const SwanTarget *target=swan_target_find("x86_64");
 for(int i=index;i<argc;i++){
  if(!strcmp(argv[i],"--low-resource"))compile_jobs=1;
  else if(!strcmp(argv[i],"--jobs")&&i+1<argc){char *end=NULL;errno=0;unsigned long jobs=strtoul(argv[++i],&end,10);if(errno||!end||*end||jobs!=1){fprintf(stderr,"swan: this compiler currently supports only --jobs 1; parallel compilation is not implemented\n");SWAN_RETURN(2);}compile_jobs=(size_t)jobs;}
  else if(!strcmp(argv[i],"--emit-asm"))emit_asm=true;
  else if(!strcmp(argv[i],"--lazy-imports"))lazy_imports=true;
  else if(!strcmp(argv[i],"--no-heap"))no_heap=true;
  else if(!strcmp(argv[i],"--profile"))swan_profile_enabled=true;
  else if(!strcmp(argv[i],"--warnings-as-errors"))warnings_as_errors=true;
  else if(!strcmp(argv[i],"--stdlib")&&i+1<argc){stdlib_dir=argv[++i];module_paths[0]=stdlib_dir;}
  else if((!strcmp(argv[i],"--module-path")||!strcmp(argv[i],"-I"))&&i+1<argc){if(module_path_count>=64){fprintf(stderr,"swan: too many module search paths (maximum 64)\n");SWAN_RETURN(2);}module_paths[module_path_count++]=argv[++i];}
  else if(!strcmp(argv[i],"-o")&&i+1<argc)output=argv[++i];
  else if(!strcmp(argv[i],"--link")&&i+1<argc){if(!swan_append_input(&link_args,&link_arg_count,&link_arg_capacity,argv[++i])){fprintf(stderr,"swan: out of memory recording linker input\n");SWAN_RETURN(2);}}
  else if((!strcmp(argv[i],"-L")||!strcmp(argv[i],"-l"))&&i+1<argc){if(!swan_append_input(&link_args,&link_arg_count,&link_arg_capacity,argv[i])||!swan_append_input(&link_args,&link_arg_count,&link_arg_capacity,argv[++i]))SWAN_RETURN(2);}
  else if((!strncmp(argv[i],"-L",2)||!strncmp(argv[i],"-l",2))&&argv[i][2]){if(!swan_append_input(&link_args,&link_arg_count,&link_arg_capacity,argv[i]))SWAN_RETURN(2);}
  else if(!strcmp(argv[i],"--target")&&i+1<argc){
   const char *name=argv[++i];target=swan_target_find(name);
   if(!target){fprintf(stderr,"swan: unknown target '%s'; use --list-targets\n",name);SWAN_RETURN(2);}
   if(!target->backend_available){fprintf(stderr,"swan: target '%s' (%s) is recognized but its backend is not implemented yet\n",target->name,target->triple);SWAN_RETURN(2);}
  }else{fprintf(stderr,"swan: unknown option '%s'\n",argv[i]);SWAN_RETURN(2);}
 }
 if(compile_jobs!=1){fprintf(stderr,"swan: invalid compiler worker limit\n");SWAN_RETURN(2);}
 if(link_arg_count&&target->architecture!=SWAN_ARCH_X86_64){fprintf(stderr,"swan: linker inputs are currently supported only by the native x86_64 executable path\n");SWAN_RETURN(2);}
 if(emit_asm)mode=SWAN_EMIT;
 if(no_heap&&target->architecture!=SWAN_ARCH_X86_64){fprintf(stderr,"swan: --no-heap is currently supported only by the native x86-64 backend\\n");SWAN_RETURN(2);}
 if(no_heap&&lazy_imports){fprintf(stderr,"swan: --no-heap cannot be combined with runtime-lazy imports\\n");SWAN_RETURN(2);}
 swan_profile_reset();
 swan_optimizer_reset();
 if(c_input_count&&target->architecture!=SWAN_ARCH_X86_64){
  fprintf(stderr,"swanc: C source assembly bundling is currently supported only for native x86_64\n");SWAN_RETURN(2);
 }
 SwanError err={0};TokenStream ts={0};char *source=NULL;
 Program p={0};p.codegen_module_index=-1;p.codegen_source_index=-1;p.bundle_imports=!lazy_imports;p.no_heap=no_heap;
 uint64_t stage=0;
 for(size_t source_index=0;source_index<swan_input_count;source_index++){
  stage=swan_profile_start();
  source=swan_read_file(swan_inputs[source_index]);
  swan_profile_end(SWAN_PROFILE_READ,stage);
  if(!source){perror(swan_inputs[source_index]);swan_program_destroy(&p);SWAN_RETURN(1);}
  memset(&ts,0,sizeof(ts));
  stage=swan_profile_start();
  if(!swan_lex(source,&ts,&err)){
   fprintf(stderr,"swan:%s:%u:%u: %s\n",swan_inputs[source_index],err.line,err.col,err.message);
   free(ts.items);free(ts.text_storage);free(source);swan_program_destroy(&p);SWAN_RETURN(1);
  }
  swan_profile_end(SWAN_PROFILE_LEX,stage);
  swan_token_stream_set_source_name(&ts,swan_inputs[source_index]);
  if(!swan_program_add_source(&p,swan_inputs[source_index],source,ts.text_storage)){
   fprintf(stderr,"swan: out of memory retaining source file '%s'\n",swan_inputs[source_index]);
   free(ts.items);free(ts.text_storage);free(source);swan_program_destroy(&p);SWAN_RETURN(1);
  }
  source=NULL;ts.text_storage=NULL;p.current_source_index=source_index;
  stage=swan_profile_start();
  if(!swan_parse(&ts,&p,&err)){
   fprintf(stderr,"swan:%s:%u:%u: %s\n",swan_inputs[source_index],err.line,err.col,err.message);
   free(ts.items);swan_program_destroy(&p);SWAN_RETURN(1);
  }
  swan_profile_end(SWAN_PROFILE_PARSE,stage);
  free(ts.items);ts.items=NULL;
 }
 p.source_function_count=p.count;
 if(!swan_import_c_headers(&p,&err)){fprintf(stderr,"swan: %s\n",err.message);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(1);}
 char **module_sources=NULL,**module_texts=NULL;size_t module_count=0,module_source_capacity=0,module_text_capacity=0;
 stage=swan_profile_start();
 if(!swan_load_imports(&p,module_paths,module_path_count,&module_sources,&module_texts,&module_count,&module_source_capacity,&module_text_capacity,&err)){
  swan_print_diagnostic("swan",&err);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(1);
 }
 swan_profile_end(SWAN_PROFILE_IMPORTS,stage);
 stage=swan_profile_start();
 if(!swan_semantic_check(&p,&err)){swan_print_diagnostic("swan",&err);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(1);}
 swan_profile_end(SWAN_PROFILE_SEMA,stage);
 stage=swan_profile_start();
 if(!swan_compute_reachability(&p,&err)){fprintf(stderr,"swan: %s\\n",err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(1);}swan_profile_end(SWAN_PROFILE_REACHABILITY,stage);
 swan_collect_warnings(&p);
 size_t warning_count=swan_print_warnings(&p);
 if(warnings_as_errors&&warning_count){
  fprintf(stderr,"swan: compilation stopped: %zu warning(s) treated as errors\n",warning_count);
  swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(1);
 }
 if(target->architecture!=SWAN_ARCH_X86_64){
  for(size_t i=0;i<p.count;i++)if(p.function_reachable[i]&&p.functions[i]->external){
   fprintf(stderr,"swan: external C ABI calls are currently supported only by the x86_64 System V backend\n");
   swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(2);
  }
 }
 bool has_lazy=swan_has_lazy_imports(&p);
 if(!has_lazy&&mode==SWAN_EMIT&&
    (p.source_count>1||c_input_count>0)){
  const char *directory=output?output:"asm";
  bool emitted=swan_emit_project_sources(&p,directory,target,mode,c_inputs,c_input_count,&err);
  if(!emitted)swan_print_diagnostic("swan",&err);
  swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);
  SWAN_RETURN(emitted?0:1);
 }
 if(target->architecture==SWAN_ARCH_WASM64){
  if(mode!=SWAN_BUILD&&mode!=SWAN_EMIT){fprintf(stderr,"swan: WASM64 output is a WebAssembly module; run it with a WebAssembly memory64 runtime\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(2);}
  if(has_lazy){fprintf(stderr,"swan: WASM64 backend does not yet support runtime-lazy imports\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(2);}
  SwanSVMOutput compiled={0};
  bool backend_ok=swan_svm_codegen(&p,target,&compiled,&err);
  if(!backend_ok){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"SVM backend failed");
   free(compiled.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(1);
  }
  const char *dest=output?output:(target->architecture==SWAN_ARCH_WASM64?"a64.wasm":"a.wasm");FILE *wf=fopen(dest,"wb");
  bool saved=wf&&fwrite(compiled.data,1,compiled.length,wf)==compiled.length;
  if(wf&&fclose(wf)!=0)saved=false;
  if(!saved)fprintf(stderr,"swan: failed to write WebAssembly module '%s'\n",dest);
  free(compiled.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(saved?0:1);
 }
 if(target->architecture==SWAN_ARCH_AARCH64){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){fprintf(stderr,"swan: AArch64 cross-target output is an ELF object; execute it on an AArch64 system after linking\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(2);}

  SwanSVMOutput compiled={0};
  bool generated=swan_svm_codegen(&p,target,&compiled,&err);
  Text assembly={.data=compiled.data,.len=compiled.length};
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"SVM backend failed");free(assembly.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(1);
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"aarch64.s":"aarch64.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-a64-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=aarch64-unknown-linux-gnu","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: AArch64 assembly failed; install a Clang toolchain with AArch64 support\n");
  }
  free(assembly.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(result);
 }
 if(mode==SWAN_EMIT&&has_lazy){fprintf(stderr,"swan: --emit-asm cannot bundle runtime-lazy modules; use 'swan build' or 'swan run'\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(2);}
 if(!has_lazy&&(p.source_count>1||c_input_count>0)&&target->architecture==SWAN_ARCH_X86_64){
  char executable[1024];bool temporary=output==NULL;int name_length;
  if(output)name_length=snprintf(executable,sizeof(executable),"%s",output);
  else if(mode==SWAN_BUILD)name_length=snprintf(executable,sizeof(executable),"a.out");
  else name_length=snprintf(executable,sizeof(executable),"/tmp/swan-run-%ld",(long)getpid());
  if(name_length<0||(size_t)name_length>=sizeof(executable)){
   fprintf(stderr,"swan: output path is too long\n");
   swan_release_modules(module_sources,module_texts,module_count);swan_program_destroy(&p);SWAN_RETURN(1);
  }
  char **objects=NULL;size_t object_count=0;
  bool compiled_objects=swan_compile_project_objects(&p,&objects,&object_count,&err);
  int result=compiled_objects?swan_link_project_objects(objects,object_count,c_inputs,c_input_count,link_args,link_arg_count,executable):1;
  if(!compiled_objects)fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"failed to compile project translation units");
  if(objects){for(size_t i=0;i<object_count;i++){if(objects[i]){unlink(objects[i]);free(objects[i]);}}free(objects);}
  swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);
  if(result!=0){if(temporary&&mode!=SWAN_BUILD)unlink(executable);SWAN_RETURN(result<0?1:result);}
  if(mode==SWAN_BUILD)SWAN_RETURN(0);
  char *run_args[]={executable,NULL};result=swan_exec(run_args,false);
  if(temporary)unlink(executable);
  SWAN_RETURN(result<0?1:result);
 }
 char exe[1024];bool temporary_output=output==NULL;int exe_len;
 if(output)exe_len=snprintf(exe,sizeof(exe),"%s",output);
 else if(mode==SWAN_BUILD)exe_len=snprintf(exe,sizeof(exe),"a.out");
 else exe_len=snprintf(exe,sizeof(exe),"/tmp/swan-run-%ld",(long)getpid());
 if(exe_len<0||(size_t)exe_len>=sizeof(exe)){fprintf(stderr,"swan: output path is too long\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);SWAN_RETURN(1);}
 SwanSVMOutput compiled={0};
 if(!swan_svm_codegen(&p,target,&compiled,&err)){swan_print_diagnostic("swan",&err);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(compiled.data);SWAN_RETURN(1);}
 Text assembly={.data=compiled.data,.len=compiled.length};
 if(has_lazy&&!swan_build_lazy_modules(&p,exe,&err)){fprintf(stderr,"swan: %s\n",err.message);swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(1);}
 char runtime_source[64]={0};if(has_lazy&&!swan_write_lazy_runtime(runtime_source)){fprintf(stderr,"swan: cannot create lazy import runtime support\n");swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(1);}
 if(mode==SWAN_EMIT){
  const char *dest=output?output:"a.s";FILE *of=fopen(dest,"wb");
  if(!of){perror(dest);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(1);}
  stage=swan_profile_start();bool ok=swan_emit_assembly(of,&assembly);
  if(ok&&c_input_count)ok=swan_append_c_assembly(of,c_inputs,c_input_count);
  if(fclose(of)!=0)ok=false;
  swan_profile_end(SWAN_PROFILE_ASM_WRITE,stage);
  if(!ok){fprintf(stderr,"swan: failed to write assembly output '%s'\n",dest);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(1);}
  if(runtime_source[0])unlink(runtime_source);
  swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(0);
 }
 char temp[]="/tmp/swan-XXXXXX";int fd=mkstemp(temp);
 if(fd<0){perror("mkstemp");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(1);}
 FILE *af=fdopen(fd,"wb");
 if(!af){close(fd);unlink(temp);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(1);}
 stage=swan_profile_start();bool written=swan_emit_assembly(af,&assembly);
 if(written&&c_input_count)written=swan_append_c_assembly(af,c_inputs,c_input_count);
 if(fclose(af)!=0)written=false;
 swan_profile_end(SWAN_PROFILE_ASM_WRITE,stage);
 if(!written){fprintf(stderr,"swan: failed to write temporary assembly\n");unlink(temp);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(1);}
 int result=0;
 if(has_lazy){
  if(link_arg_count>SIZE_MAX-16||link_arg_count+16>SIZE_MAX/sizeof(char*)){fprintf(stderr,"swan: linker argument vector size overflow\\n");unlink(temp);unlink(runtime_source);SWAN_RETURN(1);}
  char **cc_args=calloc(link_arg_count+16,sizeof(*cc_args));
  if(!cc_args){fprintf(stderr,"swan: out of memory building linker command\\n");unlink(temp);unlink(runtime_source);SWAN_RETURN(1);}
  size_t cc_count=0;cc_args[cc_count++]="cc";cc_args[cc_count++]="-rdynamic";cc_args[cc_count++]="-x";cc_args[cc_count++]="assembler";cc_args[cc_count++]=temp;
  cc_args[cc_count++]="-x";cc_args[cc_count++]="c";cc_args[cc_count++]=runtime_source;cc_args[cc_count++]="-ldl";cc_args[cc_count++]="-x";cc_args[cc_count++]="none";
  for(size_t i=0;i<link_arg_count;i++)cc_args[cc_count++]=(char*)link_args[i];
  cc_args[cc_count++]="-o";cc_args[cc_count++]=exe;cc_args[cc_count]=NULL;
  result=swan_exec(cc_args,true);free(cc_args);unlink(temp);unlink(runtime_source);
  if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);if(temporary_output)swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(result<0?1:result);}
 }else{
  result=swan_link_assembly_with_args("cc",temp,exe,link_args,link_arg_count);unlink(temp);
  if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(result<0?1:result);}
 }
 if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(result<0?1:result);}
 if(mode==SWAN_BUILD){swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);SWAN_RETURN(0);}
 char *run_args[]={exe,NULL};stage=swan_profile_start();result=swan_exec(run_args,false);swan_profile_end(SWAN_PROFILE_RUN,stage);
 if(temporary_output){unlink(exe);if(has_lazy)swan_remove_lazy_modules(exe,&p);}
 swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);
 SWAN_RETURN(result<0?1:result);
}

#undef SWAN_RETURN

int main(int argc,char **argv){int result=swan_run(argc,argv);swan_profile_print();return result;}
