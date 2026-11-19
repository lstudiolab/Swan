#define _POSIX_C_SOURCE 200809L
#include "brew/compiler.h"
#include "../../lib/Driver/SIC.h"
#include <glob.h>

/* The executable entry point owns command-line orchestration. */
typedef enum {
 BREW_RUN_DEFAULT, BREW_BUILD, BREW_RUN, BREW_EMIT
} BrewDriverMode;
static bool brew_has_suffix(const char *path,const char *suffix){
 size_t a=strlen(path),b=strlen(suffix);return a>=b&&!strcmp(path+a-b,suffix);
}
static void brew_print_diagnostic(const char *prefix,const BrewError *error){
 if(!error)return;
 if(error->source_name&&error->line)
  fprintf(stderr,"%s:%s:%u:%u: %s\n",prefix,error->source_name,error->line,error->col,error->message);
 else if(error->line)
  fprintf(stderr,"%s:%u:%u: %s\n",prefix,error->line,error->col,error->message);
 else
  fprintf(stderr,"%s: %s\n",prefix,error->message);
}
static bool brew_append_input(const char ***items,size_t *count,size_t *capacity,const char *path){
 if(!items||!count||!capacity||!path||*count==SIZE_MAX||
    !brew_reserve_array((void**)items,capacity,*count+1,sizeof(**items)))return false;
 (*items)[(*count)++]=path;return true;
}
static bool brew_collect_directory(const char *directory,const char ***brew,size_t *sn,size_t *sc,
                                   const char ***c,size_t *cn,size_t *cc){
 char pattern[4096];glob_t matches={0};int n=snprintf(pattern,sizeof(pattern),"%s/*.brew",directory);
 if(n<0||(size_t)n>=sizeof(pattern))return false;
 int result=glob(pattern,0,NULL,&matches);
 if(result!=0&&result!=GLOB_NOMATCH){globfree(&matches);return false;}
 for(size_t i=0;i<matches.gl_pathc;i++){
  char *path=strdup(matches.gl_pathv[i]);
  if(!path||!brew_append_input(brew,sn,sc,path)){free(path);globfree(&matches);return false;}
 }
 globfree(&matches);memset(&matches,0,sizeof(matches));
 n=snprintf(pattern,sizeof(pattern),"%s/*.c",directory);
 if(n<0||(size_t)n>=sizeof(pattern))return false;
 result=glob(pattern,0,NULL,&matches);
 if(result!=0&&result!=GLOB_NOMATCH){globfree(&matches);return false;}
 for(size_t i=0;i<matches.gl_pathc;i++){
  char *path=strdup(matches.gl_pathv[i]);
  if(!path||!brew_append_input(c,cn,cc,path)){free(path);globfree(&matches);return false;}
 }
 globfree(&matches);return true;
}
static bool brew_append_c_assembly(FILE *output,const char *const *sources,size_t count){
 for(size_t i=0;i<count;i++){
  char temp[]="/tmp/brewc-c-XXXXXX";int fd=mkstemp(temp);if(fd<0)return false;
  if(close(fd)!=0){unlink(temp);return false;}
  char *args[]={"cc","-S","-masm=intel","-o",temp,(char*)sources[i],NULL};
  int result=brew_exec(args,true);if(result!=0){unlink(temp);return false;}
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
static bool brew_make_output_dir(const char *directory){
 struct stat info;
 if(mkdir(directory,0755)==0)return true;
 return errno==EEXIST&&stat(directory,&info)==0&&S_ISDIR(info.st_mode);
}
static const char *brew_source_basename(const char *path){
 const char *base=strrchr(path,'/');
 return base?base+1:path;
}
static bool brew_source_output_path(char *out,size_t capacity,const char *directory,
                                    const char *source_path,const char *extension){
 const char *base=brew_source_basename(source_path);
 size_t length=strlen(base);
 if(length>=5&&!strcmp(base+length-5,".brew"))length-=5;
 if(length>(size_t)INT_MAX)return false;
 int n=snprintf(out,capacity,"%s/%.*s%s",directory,(int)length,base,extension);
 if(n<0||(size_t)n>=capacity)return false;
 return true;
}
static bool brew_emit_project_sources(Program *program,const char *directory,
                                      const BrewTarget *target,int mode,
                                      const char *const *c_inputs,size_t c_count,
                                      BrewError *error){
 (void)mode;
 if(!brew_make_output_dir(directory))return false;
 const char *extension=target->architecture==BREW_ARCH_WASM64?".wasm":".s";
 program->codegen_source_index=-1;
 program->codegen_include_external_functions=target->architecture!=BREW_ARCH_WASM64;
 program->codegen_force_ast=target->architecture==BREW_ARCH_X86_64&&
                            (program->no_heap||brew_program_requires_ast_compatibility(program));
 for(size_t i=0;i<program->source_count;i++){
  if(i>(size_t)INT_MAX){brew_error(error,NULL,"source index exceeds code-generation representation");goto fail;}
  char path[4096];
  if(!brew_source_output_path(path,sizeof(path),directory,
                              program->source_files[i].path,extension)){
   brew_error(error,NULL,"source output path is too long for '%s'",program->source_files[i].path);goto fail;
  }
  for(size_t prior=0;prior<i;prior++){
   char previous[4096];
   if(!brew_source_output_path(previous,sizeof(previous),directory,
                               program->source_files[prior].path,extension))goto fail;
   if(!strcmp(path,previous)){
    char unique[4096];
    const char *base=brew_source_basename(program->source_files[i].path);
    size_t length=strlen(base);if(length>=5&&!strcmp(base+length-5,".brew"))length-=5;
    if(length>(size_t)INT_MAX)goto fail;
    int n=snprintf(unique,sizeof(unique),"%s/%.*s_%zu%s",directory,(int)length,base,i,extension);
    if(n<0||(size_t)n>=sizeof(unique))goto fail;
    snprintf(path,sizeof(path),"%s",unique);break;
   }
  }
  program->codegen_source_index=(int)i;
  BrewSVMOutput generated={0};
  if(!brew_svm_codegen(program,target,&generated,error))goto fail;
  bool ok=false;
  if(generated.kind==BREW_SVM_OUTPUT_WASM){
   FILE *file=fopen(path,"wb");
   ok=file&&fwrite(generated.data,1,generated.length,file)==generated.length;
   if(file&&fclose(file)!=0)ok=false;
  }else{
   Text assembly={.data=generated.data,.len=generated.length};
   FILE *file=fopen(path,"wb");
   ok=file&&brew_emit_assembly(file,&assembly);
   if(file&&fclose(file)!=0)ok=false;
  }
  free(generated.data);
  if(!ok){brew_error(error,NULL,"cannot write translation unit output '%s'",path);goto fail;}
 }
 program->codegen_source_index=-1;program->codegen_force_ast=false;program->codegen_include_external_functions=false;
 for(size_t i=0;i<c_count;i++){
  char path[4096];
  int n=snprintf(path,sizeof(path),"%s/native_%zu.s",directory,i);
  if(n<0||(size_t)n>=sizeof(path))goto fail;
  FILE *file=fopen(path,"wb");
  bool ok=file&&brew_append_c_assembly(file,&c_inputs[i],1);
  if(file&&fclose(file)!=0)ok=false;
  if(!ok){brew_error(error,NULL,"cannot write C source assembly '%s'",path);goto fail;}
 }
 return true;
fail:
 program->codegen_source_index=-1;program->codegen_force_ast=false;program->codegen_include_external_functions=false;
 return false;
}
static bool brew_compile_project_objects(Program *program,char ***objects_out,
                                         size_t *object_count_out,BrewError *error){
 if(!program||!objects_out||!object_count_out)return false;
 *objects_out=NULL;*object_count_out=0;
 program->codegen_source_index=-1;
 program->codegen_include_external_functions=true;
 program->codegen_force_ast=program->no_heap||brew_program_requires_ast_compatibility(program);
 if(program->source_count>SIZE_MAX/sizeof(char*)){
  brew_error(error,NULL,"project object list size overflow");return false;
 }
 char **objects=calloc(program->source_count,sizeof(*objects));
 if(program->source_count&&!objects){brew_error(error,NULL,"out of memory creating project object list");return false;}
 for(size_t i=0;i<program->source_count;i++){
  if(i>(size_t)INT_MAX){brew_error(error,NULL,"source index exceeds code-generation representation");goto fail;}
  char assembly_path[]="/tmp/brew-tu-XXXXXX";
  int assembly_fd=mkstemp(assembly_path);
  if(assembly_fd<0){brew_error(error,NULL,"cannot create temporary translation-unit assembly");goto fail;}
  program->codegen_source_index=(int)i;
  BrewSVMOutput generated={0};
  if(!brew_svm_codegen(program,brew_target_find("x86_64"),&generated,error)){
   program->codegen_source_index=-1;close(assembly_fd);unlink(assembly_path);goto fail;
  }
  program->codegen_source_index=-1;
  FILE *assembly_file=fdopen(assembly_fd,"wb");
  bool written=assembly_file&&brew_emit_assembly(assembly_file,
      &(Text){.data=generated.data,.len=generated.length});
  if(assembly_file&&fclose(assembly_file)!=0)written=false;
  else if(!assembly_file)close(assembly_fd);
  free(generated.data);
  if(!written){unlink(assembly_path);brew_error(error,NULL,"cannot write temporary assembly for '%s'",program->source_files[i].path);goto fail;}
  char object_path[]="/tmp/brew-obj-XXXXXX";
  int object_fd=mkstemp(object_path);
  if(object_fd<0){unlink(assembly_path);brew_error(error,NULL,"cannot create temporary project object");goto fail;}
  close(object_fd);
  char *args[]={"cc","-c","-x","assembler",assembly_path,"-o",object_path,NULL};
  int result=brew_exec(args,true);unlink(assembly_path);
  if(result!=0){unlink(object_path);brew_error(error,NULL,"assembler failed for source file '%s'",program->source_files[i].path);goto fail;}
  objects[i]=malloc(sizeof(object_path));
  if(!objects[i]){unlink(object_path);brew_error(error,NULL,"out of memory retaining project object path");goto fail;}
  memcpy(objects[i],object_path,sizeof(object_path));
  (*object_count_out)++;
 }
 *objects_out=objects;program->codegen_source_index=-1;program->codegen_force_ast=false;program->codegen_include_external_functions=false;return true;
fail:
 for(size_t i=0;i<program->source_count;i++){if(objects[i]){unlink(objects[i]);free(objects[i]);}}
 free(objects);return false;
}
static int brew_link_project_objects(char *const *objects,size_t object_count,
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
 int result=brew_exec(args,true);free(args);return result;
}
static void brew_release_cli_inputs(const char **brew_inputs,size_t brew_count,
                                      const char **c_inputs,size_t c_count,
                                      const char **link_args,bool owned_paths){
 if(owned_paths){
  for(size_t i=0;i<brew_count;i++)free((void*)brew_inputs[i]);
  for(size_t i=0;i<c_count;i++)free((void*)c_inputs[i]);
 }
 free((void*)brew_inputs);free((void*)c_inputs);free((void*)link_args);
}

static bool brew_build_all_targets(Program *program,const char *directory,
                                   const char *source_name,BrewError *error){
 if(!program||!directory||!source_name||!brew_make_output_dir(directory))return false;
 const char *base=brew_source_basename(source_name);
 size_t length=strlen(base);
 if(length>=5&&!strcmp(base+length-5,".brew"))length-=5;
 if(!length||length>(size_t)INT_MAX)return false;
 char native_path[4096],arm_path[4096],wasm_path[4096];
 int a=snprintf(native_path,sizeof(native_path),"%s/%.*s-x86_64",directory,(int)length,base);
 int b=snprintf(arm_path,sizeof(arm_path),"%s/%.*s-arm64.o",directory,(int)length,base);
 int c=snprintf(wasm_path,sizeof(wasm_path),"%s/%.*s-wasm64.wasm",directory,(int)length,base);
 if(a<0||b<0||c<0||(size_t)a>=sizeof(native_path)||(size_t)b>=sizeof(arm_path)||(size_t)c>=sizeof(wasm_path)){
  brew_error(error,NULL,"target output path is too long");return false;
 }
 unlink(native_path);unlink(arm_path);unlink(wasm_path);
 char **objects=NULL;size_t object_count=0;
 if(!brew_compile_project_objects(program,&objects,&object_count,error))return false;
 int linked=brew_link_project_objects(objects,object_count,NULL,0,NULL,0,native_path);
 for(size_t i=0;i<object_count;i++){if(objects[i]){unlink(objects[i]);free(objects[i]);}}
 free(objects);
 if(linked!=0){brew_error(error,NULL,"x86-64 link failed while building target matrix");goto fail;}
 const BrewTarget *arm=brew_target_find("arm64");
 program->codegen_source_index=-1;
 program->codegen_force_ast=false;
 program->codegen_include_external_functions=false;
 BrewSVMOutput generated={0};
 if(!brew_svm_codegen(program,arm,&generated,error))goto fail;
 char assembly_path[]="/tmp/brew-all-arm-XXXXXX";
 int fd=mkstemp(assembly_path);
 if(fd<0){free(generated.data);brew_error(error,NULL,"cannot create temporary ARM64 assembly");goto fail;}
 FILE *assembly=fdopen(fd,"wb");

 bool assembly_ok=assembly&&brew_emit_assembly(assembly,
     &(Text){.data=generated.data,.len=generated.length});
 if(assembly&&fclose(assembly)!=0)assembly_ok=false;
 else if(!assembly)close(fd);
 free(generated.data);
 if(!assembly_ok){unlink(assembly_path);brew_error(error,NULL,"cannot write temporary ARM64 assembly");goto fail;}
 char *arm_args[]={"clang","--target=aarch64-unknown-linux-gnu","-c","-x","assembler",assembly_path,"-o",arm_path,NULL};
 int arm_result=brew_exec(arm_args,true);
 unlink(assembly_path);
 if(arm_result!=0){brew_error(error,NULL,"ARM64 object generation failed; install Clang with AArch64 support");goto fail;}
 const BrewTarget *wasm=brew_target_find("wasm64");
 memset(&generated,0,sizeof(generated));
 if(!brew_svm_codegen(program,wasm,&generated,error))goto fail;
 FILE *wf=fopen(wasm_path,"wb");
 bool wasm_ok=wf&&fwrite(generated.data,1,generated.length,wf)==generated.length;
 if(wf&&fclose(wf)!=0)wasm_ok=false;
 free(generated.data);
 if(!wasm_ok){brew_error(error,NULL,"cannot write WASM64 module '%s'",wasm_path);goto fail;}
 printf("Built x86-64 executable: %s\n",native_path);
 printf("Built ARM64 ELF object: %s\n",arm_path);
 printf("Built WASM64 module: %s\n",wasm_path);
 return true;
fail:
 unlink(native_path);unlink(arm_path);unlink(wasm_path);
 return false;
}

static int brew_run_wasm_file(const char *path,int argument_count,char **arguments){
 const char *engine=getenv("BREW_WASM_ENGINE");
 if(!engine||!*engine)engine="wasmtime";
 if(argument_count<0||(size_t)argument_count>SIZE_MAX/sizeof(char*)-8)return 1;

 /*
  * SIC selects execution by host. Wasmtime's run command performs the JIT
  * compilation from portable WASM to the current native CPU (x86-64/ARM64).
  * A WebAssembly-hosted build must leave the module as WASM; browser.js is
  * the browser entry point and uses the browser's own WebAssembly engine.
  */
 BrewSICHost host=brew_sic_detect_host();
 if(getenv("BREW_SIC_DEBUG")){
  const char *architecture=host.architecture==BREW_SIC_ARCH_X86_64?"x86-64":
      host.architecture==BREW_SIC_ARCH_ARM64?"ARM64":"unknown";
  fprintf(stderr,"SIC: environment=%s architecture=%s\n",
      host.environment==BREW_SIC_ENV_WEBASSEMBLY?"WebAssembly":"native",architecture);
 }
 if(host.environment==BREW_SIC_ENV_WEBASSEMBLY){
  fprintf(stderr,"brew: SIC detected a WebAssembly host; load the module with runtime/wasm64/browser.js instead of native recompilation\n");
  return 2;
 }
 if(host.architecture!=BREW_SIC_ARCH_X86_64&&host.architecture!=BREW_SIC_ARCH_ARM64){
  fprintf(stderr,"brew: SIC supports native JIT on x86-64 and ARM64; retained portable WASM module at '%s'\n",path);
  return 2;
 }
 bool temporary_wasm=!strncmp(path,"/tmp/brew-wasm-run-",19);
 char **args=calloc((size_t)argument_count+8,sizeof(*args));
 if(!args){fprintf(stderr,"brew: out of memory preparing SIC runtime invocation\n");return 1;}
 size_t n=0;
 args[n++]=(char*)engine;
 args[n++]="run";
 args[n++]="--wasm";
 args[n++]="memory64=y";
 args[n++]="--invoke";
 args[n++]="main";
 args[n++]=(char*)path;
 for(int i=0;i<argument_count;i++)args[n++]=arguments[i];
 args[n]=NULL;
 int result=brew_exec(args,true);
 if(result<0){
  if(errno==ENOENT)fprintf(stderr,"brew: WebAssembly runtime '%s' was not found; install Wasmtime or set BREW_WASM_ENGINE\n",engine);
  else fprintf(stderr,"brew: could not start WebAssembly runtime '%s': %s\n",engine,strerror(errno));
  result=127;
 }
 if(temporary_wasm&&result==0)unlink(path);
 else if(temporary_wasm&&result!=0)
  fprintf(stderr,"brew: SIC retained temporary WASM module '%s' because native JIT execution did not complete successfully\n",path);
 free(args);
 return result;
}

static int brew_run(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--version")){puts("Brew " BREW_VERSION);return 0;}
 if(argc==2&&!strcmp(argv[1],"--list-targets")){brew_target_list(stdout);return 0;}
 if(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){printf("Brew %s\nUsage: brewc [compile|build|run] <file.brew|file.wasm|directory> [more.brew ...] [native.c ...] [--low-resource | --jobs 1] [--target x86_64|arm64|wasm64|all] [--module-path directory ...] [--lazy-imports] [--no-heap] [--warnings-as-errors] [--link object] [-L dir] [-l library] [-o output]\n  compile emits portable WASM64; run uses SIC to select native host JIT or browser WASM execution.\n  --target all builds one shared source project for x86-64, ARM64, and WASM64 into the -o directory.\n  'brew run file.wasm' executes the exported main function with Wasmtime memory64 enabled.\n",BREW_VERSION);return 0;}
 BrewDriverMode mode=BREW_RUN_DEFAULT;
 bool sic_compile_command=false;
 int index=1;
 if(index<argc&&!strcmp(argv[index],"build")){mode=BREW_BUILD;index++;}
 else if(index<argc&&!strcmp(argv[index],"compile")){mode=BREW_BUILD;sic_compile_command=true;index++;}
 else if(index<argc&&!strcmp(argv[index],"run")){mode=BREW_RUN;index++;}
 if(index<argc&&!strcmp(argv[index],"--profile")){brew_profile_enabled=true;index++;}
 if(index>=argc){fprintf(stderr,"Brew %s\nUsage: brewc [compile|build|run] <file.brew|directory> [more.brew ...] [--low-resource | --jobs 1] [--emit-asm] [--target x86_64|arm64|wasm64|all] [--stdlib directory] [--module-path directory ...] [-o output]\n",BREW_VERSION);return 2;}
 if(index<argc&&brew_has_suffix(argv[index],".wasm")){
  if(mode==BREW_BUILD){fprintf(stderr,"brew: WebAssembly modules are executed with 'brew run', not 'brew build'\n");return 2;}
  return brew_run_wasm_file(argv[index],argc-index-1,&argv[index+1]);
 }
 const char **brew_inputs=NULL,**c_inputs=NULL,**link_args=NULL;
 size_t brew_input_count=0,brew_input_capacity=0,c_input_count=0,c_input_capacity=0;
 size_t link_arg_count=0,link_arg_capacity=0;
 bool directory_inputs_owned=false;
 #define BREW_RETURN(value) do { \
  brew_release_cli_inputs(brew_inputs,brew_input_count,c_inputs,c_input_count,link_args,directory_inputs_owned); \
  return (value); \
 } while(0)
 const char *first=argv[index++];struct stat first_stat;
 if(stat(first,&first_stat)==0&&S_ISDIR(first_stat.st_mode)){
  directory_inputs_owned=true;
  if(!brew_collect_directory(first,&brew_inputs,&brew_input_count,&brew_input_capacity,&c_inputs,&c_input_count,&c_input_capacity)){
   fprintf(stderr,"brewc: cannot collect sources from '%s'\n",first);BREW_RETURN(2);
  }
 }else if(brew_has_suffix(first,".brew")){if(!brew_append_input(&brew_inputs,&brew_input_count,&brew_input_capacity,first)){fprintf(stderr,"brewc: out of memory recording source input\\n");BREW_RETURN(2);}}
 else if(brew_has_suffix(first,".c")){if(!brew_append_input(&c_inputs,&c_input_count,&c_input_capacity,first)){fprintf(stderr,"brewc: out of memory recording C input\\n");BREW_RETURN(2);}}
 else {fprintf(stderr,"brewc: expected a .brew file or source directory\n");BREW_RETURN(2);}
 while(index<argc&&(brew_has_suffix(argv[index],".brew")||brew_has_suffix(argv[index],".c"))){
  if(brew_has_suffix(argv[index],".brew")){
   if(!brew_append_input(&brew_inputs,&brew_input_count,&brew_input_capacity,argv[index++])){fprintf(stderr,"brewc: out of memory recording source input\\n");BREW_RETURN(2);}
  }else{
   if(!brew_append_input(&c_inputs,&c_input_count,&c_input_capacity,argv[index++])){fprintf(stderr,"brewc: out of memory recording C input\\n");BREW_RETURN(2);}
  }
 }
 if(!brew_input_count){fprintf(stderr,"brewc: source directory contains no .brew files\n");BREW_RETURN(2);}
 const char *output=NULL,*stdlib_dir=getenv("BREW_STDLIB");
 if(!stdlib_dir||!*stdlib_dir)stdlib_dir="stdlib";
 const char *module_paths[64];size_t module_path_count=1;module_paths[0]=stdlib_dir;
 bool emit_asm=false,lazy_imports=false,warnings_as_errors=false,no_heap=false,target_all=false;size_t compile_jobs=1;
 BrewSICHost detected_host=brew_sic_detect_host();
 const char *default_target_name=sic_compile_command||detected_host.environment==BREW_SIC_ENV_WEBASSEMBLY?"wasm64":
     detected_host.architecture==BREW_SIC_ARCH_ARM64?"arm64":"x86_64";
 const BrewTarget *target=brew_target_find(default_target_name);
 for(int i=index;i<argc;i++){
  if(!strcmp(argv[i],"--low-resource"))compile_jobs=1;
  else if(!strcmp(argv[i],"--jobs")&&i+1<argc){char *end=NULL;errno=0;unsigned long jobs=strtoul(argv[++i],&end,10);if(errno||!end||*end||jobs!=1){fprintf(stderr,"brew: this compiler currently supports only --jobs 1; parallel compilation is not implemented\n");BREW_RETURN(2);}compile_jobs=(size_t)jobs;}
  else if(!strcmp(argv[i],"--emit-asm"))emit_asm=true;
  else if(!strcmp(argv[i],"--lazy-imports"))lazy_imports=true;
  else if(!strcmp(argv[i],"--no-heap"))no_heap=true;
  else if(!strcmp(argv[i],"--profile"))brew_profile_enabled=true;
  else if(!strcmp(argv[i],"--warnings-as-errors"))warnings_as_errors=true;
  else if(!strcmp(argv[i],"--stdlib")&&i+1<argc){stdlib_dir=argv[++i];module_paths[0]=stdlib_dir;}
  else if((!strcmp(argv[i],"--module-path")||!strcmp(argv[i],"-I"))&&i+1<argc){if(module_path_count>=64){fprintf(stderr,"brew: too many module search paths (maximum 64)\n");BREW_RETURN(2);}module_paths[module_path_count++]=argv[++i];}
  else if(!strcmp(argv[i],"-o")&&i+1<argc)output=argv[++i];
  else if(!strcmp(argv[i],"--link")&&i+1<argc){if(!brew_append_input(&link_args,&link_arg_count,&link_arg_capacity,argv[++i])){fprintf(stderr,"brew: out of memory recording linker input\n");BREW_RETURN(2);}}
  else if((!strcmp(argv[i],"-L")||!strcmp(argv[i],"-l"))&&i+1<argc){if(!brew_append_input(&link_args,&link_arg_count,&link_arg_capacity,argv[i])||!brew_append_input(&link_args,&link_arg_count,&link_arg_capacity,argv[++i]))BREW_RETURN(2);}
  else if((!strncmp(argv[i],"-L",2)||!strncmp(argv[i],"-l",2))&&argv[i][2]){if(!brew_append_input(&link_args,&link_arg_count,&link_arg_capacity,argv[i]))BREW_RETURN(2);}
  else if(!strcmp(argv[i],"--target")&&i+1<argc){
   const char *name=argv[++i];
   if(sic_compile_command&&strcmp(name,"wasm64")){
    fprintf(stderr,"brew: compile always produces portable WASM64; use build --target %s for an explicit legacy backend\n",name);
    BREW_RETURN(2);
   }
   if(!strcmp(name,"all")){target_all=true;target=brew_target_find("x86_64");}
   else{
    target=brew_target_find(name);
    if(!target){fprintf(stderr,"brew: unknown target '%s'; use --list-targets\n",name);BREW_RETURN(2);}
    if(!target->backend_available){fprintf(stderr,"brew: target '%s' (%s) is recognized but its backend is not implemented yet\n",target->name,target->triple);BREW_RETURN(2);}
   }
  }else{fprintf(stderr,"brew: unknown option '%s'\n",argv[i]);BREW_RETURN(2);}
 }
 if(compile_jobs!=1){fprintf(stderr,"brew: invalid compiler worker limit\n");BREW_RETURN(2);}
 if(target_all&&(mode!=BREW_BUILD||emit_asm)){fprintf(stderr,"brew: --target all requires 'build' and cannot be combined with --emit-asm\n");BREW_RETURN(2);}
 if(target_all&&(c_input_count||link_arg_count||no_heap||lazy_imports)){fprintf(stderr,"brew: --target all currently requires Brew-only sources without C inputs, linker flags, --no-heap, or --lazy-imports\n");BREW_RETURN(2);}
 bool native_arm64_target=target->architecture==BREW_ARCH_AARCH64&&detected_host.environment==BREW_SIC_ENV_NATIVE&&detected_host.architecture==BREW_SIC_ARCH_ARM64;
 if(link_arg_count&&target->architecture!=BREW_ARCH_X86_64&&!native_arm64_target){fprintf(stderr,"brew: linker inputs require a native x86-64 or ARM64 execution target\n");BREW_RETURN(2);}
 if(emit_asm)mode=BREW_EMIT;
 if(no_heap&&target->architecture!=BREW_ARCH_X86_64){fprintf(stderr,"brew: --no-heap is currently supported only by the native x86-64 backend\\n");BREW_RETURN(2);}
 if(no_heap&&lazy_imports){fprintf(stderr,"brew: --no-heap cannot be combined with runtime-lazy imports\\n");BREW_RETURN(2);}
 brew_profile_reset();
 brew_optimizer_reset();
 if(c_input_count&&target->architecture!=BREW_ARCH_X86_64){
  fprintf(stderr,"brewc: C source assembly bundling is currently supported only for native x86_64\n");BREW_RETURN(2);
 }
 BrewError err={0};TokenStream ts={0};char *source=NULL;
 Program p={0};p.codegen_module_index=-1;p.codegen_source_index=-1;p.bundle_imports=!lazy_imports;p.no_heap=no_heap;
 uint64_t stage=0;
 for(size_t source_index=0;source_index<brew_input_count;source_index++){
  stage=brew_profile_start();
  source=brew_read_file(brew_inputs[source_index]);
  brew_profile_end(BREW_PROFILE_READ,stage);
  if(!source){perror(brew_inputs[source_index]);brew_program_destroy(&p);BREW_RETURN(1);}
  memset(&ts,0,sizeof(ts));
  stage=brew_profile_start();
  if(!brew_lex(source,&ts,&err)){
   fprintf(stderr,"brew:%s:%u:%u: %s\n",brew_inputs[source_index],err.line,err.col,err.message);
   free(ts.items);free(ts.text_storage);free(source);brew_program_destroy(&p);BREW_RETURN(1);
  }
  brew_profile_end(BREW_PROFILE_LEX,stage);
  brew_token_stream_set_source_name(&ts,brew_inputs[source_index]);
  if(!brew_program_add_source(&p,brew_inputs[source_index],source,ts.text_storage)){
   fprintf(stderr,"brew: out of memory retaining source file '%s'\n",brew_inputs[source_index]);
   free(ts.items);free(ts.text_storage);free(source);brew_program_destroy(&p);BREW_RETURN(1);
  }
  source=NULL;ts.text_storage=NULL;p.current_source_index=source_index;
  stage=brew_profile_start();
  if(!brew_parse(&ts,&p,&err)){
   fprintf(stderr,"brew:%s:%u:%u: %s\n",brew_inputs[source_index],err.line,err.col,err.message);
   free(ts.items);brew_program_destroy(&p);BREW_RETURN(1);
  }
  brew_profile_end(BREW_PROFILE_PARSE,stage);
  free(ts.items);ts.items=NULL;
 }
 p.source_function_count=p.count;
 if(!brew_import_c_headers(&p,&err)){fprintf(stderr,"brew: %s\n",err.message);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);}
 char **module_sources=NULL,**module_texts=NULL;size_t module_count=0,module_source_capacity=0,module_text_capacity=0;
 stage=brew_profile_start();
 if(!brew_load_imports(&p,module_paths,module_path_count,&module_sources,&module_texts,&module_count,&module_source_capacity,&module_text_capacity,&err)){
  brew_print_diagnostic("brew",&err);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);
 }
 brew_profile_end(BREW_PROFILE_IMPORTS,stage);
 stage=brew_profile_start();
 if(!brew_semantic_check(&p,&err)){brew_print_diagnostic("brew",&err);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);}
 brew_profile_end(BREW_PROFILE_SEMA,stage);
 stage=brew_profile_start();
 if(!brew_compute_reachability(&p,&err)){fprintf(stderr,"brew: %s\\n",err.message);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);}brew_profile_end(BREW_PROFILE_REACHABILITY,stage);
 brew_collect_warnings(&p);
 size_t warning_count=brew_print_warnings(&p);
 if(warnings_as_errors&&warning_count){
  fprintf(stderr,"brew: compilation stopped: %zu warning(s) treated as errors\n",warning_count);
  brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);
 }
 if(target->architecture==BREW_ARCH_WASM64){
  for(size_t i=0;i<p.count;i++)if(p.function_reachable[i]&&p.functions[i]->external){
   fprintf(stderr,"brew: external C ABI calls are not yet supported by the WASM64 backend\n");
   brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(2);
  }
 }
 bool has_lazy=brew_has_lazy_imports(&p);
 if(target_all){
  if(has_lazy){fprintf(stderr,"brew: --target all does not yet support runtime-lazy imports\n");brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(2);}
  const char *directory=output?output:"build-all";
  bool built=brew_build_all_targets(&p,directory,first,&err);
  if(!built)brew_print_diagnostic("brew",&err);
  brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);
  BREW_RETURN(built?0:1);
 }
 if(!has_lazy&&mode==BREW_EMIT&&
    (p.source_count>1||c_input_count>0)){
  const char *directory=output?output:"asm";
  bool emitted=brew_emit_project_sources(&p,directory,target,mode,c_inputs,c_input_count,&err);
  if(!emitted)brew_print_diagnostic("brew",&err);
  brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);
  BREW_RETURN(emitted?0:1);
 }
 if(target->architecture==BREW_ARCH_WASM64){
  if(has_lazy){fprintf(stderr,"brew: WASM64 backend does not yet support runtime-lazy imports\n");brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(2);}
  bool run_after_build=mode==BREW_RUN||mode==BREW_RUN_DEFAULT;
  int result=0;
  char temporary_wasm[128];
  const char *dest=output;
  if(!dest){
   if(run_after_build){snprintf(temporary_wasm,sizeof(temporary_wasm),"/tmp/brew-wasm-run-%ld.wasm",(long)getpid());dest=temporary_wasm;}
   else dest="a.wasm";
  }
  BrewSVMOutput compiled={0};
  bool backend_ok=brew_svm_codegen(&p,target,&compiled,&err);
  if(!backend_ok){
   fprintf(stderr,"brew: %s\n",err.message[0]?err.message:"SVM backend failed");
   free(compiled.data);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);
  }
  FILE *wf=fopen(dest,"wb");
  bool saved=wf&&fwrite(compiled.data,1,compiled.length,wf)==compiled.length;
  if(wf&&fclose(wf)!=0)saved=false;
  free(compiled.data);
  if(!saved){
   fprintf(stderr,"brew: failed to write WebAssembly module '%s'\n",dest);
   brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);
  }
  if(run_after_build){
   result=brew_run_wasm_file(dest,0,NULL);
  }
  brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);
  BREW_RETURN(result);
 }
 if(target->architecture==BREW_ARCH_AARCH64){
  BrewSICHost host=brew_sic_detect_host();
  bool native_arm64=host.environment==BREW_SIC_ENV_NATIVE&&host.architecture==BREW_SIC_ARCH_ARM64;
  if((mode==BREW_RUN||mode==BREW_RUN_DEFAULT)&&!native_arm64){
   fprintf(stderr,"brew: AArch64 execution requires an ARM64 host; cross-target output must be linked and run on ARM64\n");
   brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(2);
  }
  BrewSVMOutput compiled={0};
  bool generated=brew_svm_codegen(&p,target,&compiled,&err);
  Text assembly={.data=compiled.data,.len=compiled.length};
  if(!generated){
   fprintf(stderr,"brew: %s\n",err.message[0]?err.message:"SVM backend failed");free(assembly.data);
   brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);
  }
  int result=0;
  if(mode==BREW_EMIT){
   const char *dest=output?output:"aarch64.s";
   FILE *of=fopen(dest,"wb");bool ok=of&&brew_emit_assembly(of,&assembly);
   if(of&&fclose(of)!=0)ok=false;
   if(!ok){fprintf(stderr,"brew: cannot write assembly '%s'\n",dest);result=1;}
  }else if(native_arm64){
   char executable[1024];
   bool temporary=output==NULL&&mode!=BREW_BUILD;
   int name_length=output?snprintf(executable,sizeof(executable),"%s",output):
       mode==BREW_BUILD?snprintf(executable,sizeof(executable),"a.out"):
       snprintf(executable,sizeof(executable),"/tmp/brew-arm64-run-%ld",(long)getpid());
   if(name_length<0||(size_t)name_length>=sizeof(executable)){
    fprintf(stderr,"brew: output path is too long\n");result=1;
   }else{
    char temp[]="/tmp/brew-arm64-XXXXXX";int fd=mkstemp(temp);
    if(fd<0)result=1;
    else{
     FILE *of=fdopen(fd,"wb");bool written=of&&brew_emit_assembly(of,&assembly);
     if(written&&c_input_count)written=brew_append_c_assembly(of,c_inputs,c_input_count);
     if(of&&fclose(of)!=0)written=false;
     else if(!of)close(fd);
     if(!written){fprintf(stderr,"brew: cannot write temporary ARM64 assembly\n");result=1;}
     else result=brew_link_assembly_with_args("cc",temp,executable,link_args,link_arg_count);
     unlink(temp);
    }
    if(result==0&&mode!=BREW_BUILD){
     char *run_args[]={executable,NULL};result=brew_exec(run_args,false);
    }
    if(temporary)unlink(executable);
   }
  }else{
   const char *dest=output?output:"aarch64.o";
   char temp[]="/tmp/brew-a64-XXXXXX";int fd=mkstemp(temp);
   if(fd<0)result=1;
   else{
    FILE *of=fdopen(fd,"wb");bool ok=of&&brew_emit_assembly(of,&assembly);
    if(of&&fclose(of)!=0)ok=false;
    else if(!of)close(fd);
    if(!ok)result=1;
    else{
     char *args[]={"clang","--target=aarch64-unknown-linux-gnu","-c","-x","assembler",temp,"-o",(char*)dest,NULL};
     result=brew_exec(args,true);
    }
    unlink(temp);
   }
   if(result)fprintf(stderr,"brew: AArch64 assembly failed; install a Clang toolchain with AArch64 support\n");
  }
  free(assembly.data);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);
  BREW_RETURN(result);
 }
 if(mode==BREW_EMIT&&has_lazy){fprintf(stderr,"brew: --emit-asm cannot bundle runtime-lazy modules; use 'brew build' or 'brew run'\n");brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(2);}
 if(!has_lazy&&(p.source_count>1||c_input_count>0)&&target->architecture==BREW_ARCH_X86_64){
  char executable[1024];bool temporary=output==NULL;int name_length;
  if(output)name_length=snprintf(executable,sizeof(executable),"%s",output);
  else if(mode==BREW_BUILD)name_length=snprintf(executable,sizeof(executable),"a.out");
  else name_length=snprintf(executable,sizeof(executable),"/tmp/brew-run-%ld",(long)getpid());
  if(name_length<0||(size_t)name_length>=sizeof(executable)){
   fprintf(stderr,"brew: output path is too long\n");
   brew_release_modules(module_sources,module_texts,module_count);brew_program_destroy(&p);BREW_RETURN(1);
  }
  char **objects=NULL;size_t object_count=0;
  bool compiled_objects=brew_compile_project_objects(&p,&objects,&object_count,&err);
  int result=compiled_objects?brew_link_project_objects(objects,object_count,c_inputs,c_input_count,link_args,link_arg_count,executable):1;
  if(!compiled_objects)fprintf(stderr,"brew: %s\n",err.message[0]?err.message:"failed to compile project translation units");
  if(objects){for(size_t i=0;i<object_count;i++){if(objects[i]){unlink(objects[i]);free(objects[i]);}}free(objects);}
  brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);
  if(result!=0){if(temporary&&mode!=BREW_BUILD)unlink(executable);BREW_RETURN(result<0?1:result);}
  if(mode==BREW_BUILD)BREW_RETURN(0);
  char *run_args[]={executable,NULL};result=brew_exec(run_args,false);
  if(temporary)unlink(executable);
  BREW_RETURN(result<0?1:result);
 }
 char exe[1024];bool temporary_output=output==NULL;int exe_len;
 if(output)exe_len=snprintf(exe,sizeof(exe),"%s",output);
 else if(mode==BREW_BUILD)exe_len=snprintf(exe,sizeof(exe),"a.out");
 else exe_len=snprintf(exe,sizeof(exe),"/tmp/brew-run-%ld",(long)getpid());
 if(exe_len<0||(size_t)exe_len>=sizeof(exe)){fprintf(stderr,"brew: output path is too long\n");brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);BREW_RETURN(1);}
 BrewSVMOutput compiled={0};
 if(!brew_svm_codegen(&p,target,&compiled,&err)){brew_print_diagnostic("brew",&err);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(compiled.data);BREW_RETURN(1);}
 Text assembly={.data=compiled.data,.len=compiled.length};
 if(has_lazy&&!brew_build_lazy_modules(&p,exe,&err)){fprintf(stderr,"brew: %s\n",err.message);brew_remove_lazy_modules(exe,&p);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(1);}
 char runtime_source[64]={0};if(has_lazy&&!brew_write_lazy_runtime(runtime_source)){fprintf(stderr,"brew: cannot create lazy import runtime support\n");brew_remove_lazy_modules(exe,&p);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(1);}
 if(mode==BREW_EMIT){
  const char *dest=output?output:"a.s";FILE *of=fopen(dest,"wb");
  if(!of){perror(dest);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(1);}
  stage=brew_profile_start();bool ok=brew_emit_assembly(of,&assembly);
  if(ok&&c_input_count)ok=brew_append_c_assembly(of,c_inputs,c_input_count);
  if(fclose(of)!=0)ok=false;
  brew_profile_end(BREW_PROFILE_ASM_WRITE,stage);
  if(!ok){fprintf(stderr,"brew: failed to write assembly output '%s'\n",dest);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(1);}
  if(runtime_source[0])unlink(runtime_source);
  brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(0);
 }
 char temp[]="/tmp/brew-XXXXXX";int fd=mkstemp(temp);
 if(fd<0){perror("mkstemp");brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(1);}
 FILE *af=fdopen(fd,"wb");
 if(!af){close(fd);unlink(temp);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(1);}
 stage=brew_profile_start();bool written=brew_emit_assembly(af,&assembly);
 if(written&&c_input_count)written=brew_append_c_assembly(af,c_inputs,c_input_count);
 if(fclose(af)!=0)written=false;
 brew_profile_end(BREW_PROFILE_ASM_WRITE,stage);
 if(!written){fprintf(stderr,"brew: failed to write temporary assembly\n");unlink(temp);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(1);}
 int result=0;
 if(has_lazy){
  if(link_arg_count>SIZE_MAX-16||link_arg_count+16>SIZE_MAX/sizeof(char*)){fprintf(stderr,"brew: linker argument vector size overflow\\n");unlink(temp);unlink(runtime_source);BREW_RETURN(1);}
  char **cc_args=calloc(link_arg_count+16,sizeof(*cc_args));
  if(!cc_args){fprintf(stderr,"brew: out of memory building linker command\\n");unlink(temp);unlink(runtime_source);BREW_RETURN(1);}
  size_t cc_count=0;cc_args[cc_count++]="cc";cc_args[cc_count++]="-rdynamic";cc_args[cc_count++]="-x";cc_args[cc_count++]="assembler";cc_args[cc_count++]=temp;
  cc_args[cc_count++]="-x";cc_args[cc_count++]="c";cc_args[cc_count++]=runtime_source;cc_args[cc_count++]="-ldl";cc_args[cc_count++]="-x";cc_args[cc_count++]="none";
  for(size_t i=0;i<link_arg_count;i++)cc_args[cc_count++]=(char*)link_args[i];
  cc_args[cc_count++]="-o";cc_args[cc_count++]=exe;cc_args[cc_count]=NULL;
  result=brew_exec(cc_args,true);free(cc_args);unlink(temp);unlink(runtime_source);
  if(result!=0){if(temporary_output&&mode!=BREW_BUILD)unlink(exe);if(temporary_output)brew_remove_lazy_modules(exe,&p);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(result<0?1:result);}
 }else{
  result=brew_link_assembly_with_args("cc",temp,exe,link_args,link_arg_count);unlink(temp);
  if(result!=0){if(temporary_output&&mode!=BREW_BUILD)unlink(exe);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(result<0?1:result);}
 }
 if(result!=0){if(temporary_output&&mode!=BREW_BUILD)unlink(exe);brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(result<0?1:result);}
 if(mode==BREW_BUILD){brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);BREW_RETURN(0);}
 char *run_args[]={exe,NULL};stage=brew_profile_start();result=brew_exec(run_args,false);brew_profile_end(BREW_PROFILE_RUN,stage);
 if(temporary_output){unlink(exe);if(has_lazy)brew_remove_lazy_modules(exe,&p);}
 brew_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);brew_program_destroy(&p);free(source);free(assembly.data);
 BREW_RETURN(result<0?1:result);
}

#undef BREW_RETURN

int main(int argc,char **argv){int result=brew_run(argc,argv);brew_profile_print();return result;}
