#define _POSIX_C_SOURCE 200809L
#include "swan/compiler.h"
#include <glob.h>

/* The executable entry point owns command-line orchestration. */
static bool swan_has_suffix(const char *path,const char *suffix){
 size_t a=strlen(path),b=strlen(suffix);return a>=b&&!strcmp(path+a-b,suffix);
}
static bool swan_collect_directory(const char *directory,const char **swan,size_t *sn,
                                   const char **c,size_t *cn){
 char pattern[4096];glob_t matches={0};int n=snprintf(pattern,sizeof(pattern),"%s/*.swan",directory);
 if(n<0||(size_t)n>=sizeof(pattern))return false;
 int result=glob(pattern,0,NULL,&matches);
 if(result!=0&&result!=GLOB_NOMATCH){globfree(&matches);return false;}
 for(size_t i=0;i<matches.gl_pathc;i++){if(*sn>=128){globfree(&matches);return false;}char *path=strdup(matches.gl_pathv[i]);if(!path){globfree(&matches);return false;}swan[(*sn)++]=path;}
 globfree(&matches);memset(&matches,0,sizeof(matches));
 n=snprintf(pattern,sizeof(pattern),"%s/*.c",directory);
 if(n<0||(size_t)n>=sizeof(pattern))return false;
 result=glob(pattern,0,NULL,&matches);
 if(result!=0&&result!=GLOB_NOMATCH){globfree(&matches);return false;}
 for(size_t i=0;i<matches.gl_pathc;i++){if(*cn>=120){globfree(&matches);return false;}char *path=strdup(matches.gl_pathv[i]);if(!path){globfree(&matches);return false;}c[(*cn)++]=path;}
 globfree(&matches);return true;
}
static char *swan_read_sources(const char *const *paths,size_t count){
 char *combined=NULL;size_t used=0,capacity=0;
 for(size_t i=0;i<count;i++){
  char *part=swan_read_file(paths[i]);if(!part){free(combined);return NULL;}
  size_t n=strlen(part);
  if(n>SIZE_MAX-used-2){free(part);free(combined);errno=EOVERFLOW;return NULL;}
  size_t need=used+n+2;
  if(need>capacity){
   size_t next_capacity=capacity?capacity:4096;
   while(next_capacity<need){
    if(next_capacity>SIZE_MAX/2){next_capacity=need;break;}
    next_capacity*=2;
   }
   char *next=realloc(combined,next_capacity);
   if(!next){free(part);free(combined);return NULL;}
   combined=next;capacity=next_capacity;
  }
  memcpy(combined+used,part,n);used+=n;combined[used++]='\n';combined[used]=0;
  free(part);
 }
 if(!combined)combined=calloc(1,1);
 return combined;
}
static bool swan_append_c_assembly(FILE *output,const char *const *sources,size_t count){
 for(size_t i=0;i<count;i++){
  char temp[]="/tmp/swanc-c-XXXXXX";int fd=mkstemp(temp);if(fd<0)return false;close(fd);unlink(temp);
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
static bool swan_llvm_ir_to_assembly(const char *ir,size_t length,Text *assembly,SwanError *error){
 char ir_path[]="/tmp/swanc-ir-XXXXXX",asm_path[]="/tmp/swanc-asm-XXXXXX";
 int ir_fd=mkstemp(ir_path);if(ir_fd<0)return false;
 size_t written=0;bool ok=true;
 while(written<length){
  ssize_t n=write(ir_fd,ir+written,length-written);
  if(n<0){if(errno==EINTR)continue;ok=false;break;}
  if(n==0){ok=false;break;}
  written+=(size_t)n;
 }
 if(close(ir_fd)!=0)ok=false;
 if(!ok){unlink(ir_path);return false;}
 int asm_fd=mkstemp(asm_path);if(asm_fd<0){unlink(ir_path);return false;}
 if(close(asm_fd)!=0){unlink(ir_path);unlink(asm_path);return false;}
 char *args[]={"clang","--target=x86_64-unknown-linux-gnu","-O3","-march=x86-64","-mtune=generic","-fno-addrsig","-S","-x","ir",ir_path,"-o",asm_path,NULL};
 uint64_t stage=swan_profile_start();
 int result=swan_exec(args,true);
 swan_profile_end(SWAN_PROFILE_CODEGEN,stage);
 unlink(ir_path);
 if(result!=0){unlink(asm_path);if(error)swan_error(error,NULL,"LLVM native backend failed; install Clang with LLVM IR support");return false;}
 char *data=swan_read_file(asm_path);unlink(asm_path);
 if(!data){if(error)swan_error(error,NULL,"cannot read optimized native assembly");return false;}
 assembly->data=data;assembly->len=strlen(data);
 return true;
}
static int swan_run(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--version")){puts("Swan " SWAN_VERSION);return 0;}
 if(argc==2&&!strcmp(argv[1],"--list-targets")){swan_target_list(stdout);return 0;}
 if(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){printf("Swan %s\nUsage: swanc [build|run] <file.swan> [more.swan ...] [native.c ...] [--target <target|portable> | --target-triple <triple>] [--target-feature <feature>] [--emit-llvm] [--lazy-imports] [--link object] [-L dir] [-l library] [-o output]\n",SWAN_VERSION);return 0;}
 enum { SWAN_RUN_DEFAULT,SWAN_BUILD,SWAN_RUN,SWAN_EMIT,SWAN_EMIT_LLVM } mode=SWAN_RUN_DEFAULT;
 int index=1;
 if(index<argc&&!strcmp(argv[index],"build")){mode=SWAN_BUILD;index++;}
 else if(index<argc&&!strcmp(argv[index],"run")){mode=SWAN_RUN;index++;}
 if(index<argc&&!strcmp(argv[index],"--profile")){swan_profile_enabled=true;index++;}
 if(index>=argc){fprintf(stderr,"Swan %s\nUsage: swanc [build|run] <file.swan> [more.swan ...] [native.c ...] [--emit-asm|--emit-llvm] [--target x86_64|--target-triple triple] [--target-feature feature] [--stdlib directory] [-o output]\n",SWAN_VERSION);return 2;}
 const char *swan_inputs[128],*c_inputs[120];size_t swan_input_count=0,c_input_count=0;
 const char *first=argv[index++];struct stat first_stat;
 if(stat(first,&first_stat)==0&&S_ISDIR(first_stat.st_mode)){
  if(!swan_collect_directory(first,swan_inputs,&swan_input_count,c_inputs,&c_input_count)){
   fprintf(stderr,"swanc: cannot collect sources from '%s'\n",first);return 2;
  }
 }else if(swan_has_suffix(first,".swan"))swan_inputs[swan_input_count++]=first;
 else if(swan_has_suffix(first,".c"))c_inputs[c_input_count++]=first;
 else {fprintf(stderr,"swanc: expected a .swan file or source directory\n");return 2;}
 while(index<argc&&(swan_has_suffix(argv[index],".swan")||swan_has_suffix(argv[index],".c"))){
  if(swan_has_suffix(argv[index],".swan")){
   if(swan_input_count>=128){fprintf(stderr,"swanc: too many Swan source files\n");return 2;}
   swan_inputs[swan_input_count++]=argv[index++];
  }else{
   if(c_input_count>=120){fprintf(stderr,"swanc: too many C source files\n");return 2;}
   c_inputs[c_input_count++]=argv[index++];
  }
 }
 if(!swan_input_count){fprintf(stderr,"swanc: source directory contains no .swan files\n");return 2;}
 const char *input=swan_inputs[0],*output=NULL,*stdlib_dir=getenv("SWAN_STDLIB");
 if(!stdlib_dir||!*stdlib_dir)stdlib_dir="stdlib";
 bool emit_asm=false,emit_llvm=false,lazy_imports=false;const char *link_args[120];size_t link_arg_count=0;
 const char *target_features[32];size_t target_feature_count=0;const char *target_cpu=NULL;
 SwanTarget custom_target={0};const SwanTarget *target=swan_target_find("x86_64");
 for(int i=index;i<argc;i++){
  if(!strcmp(argv[i],"--emit-asm"))emit_asm=true;
  else if(!strcmp(argv[i],"--emit-llvm"))emit_llvm=true;
  else if(!strcmp(argv[i],"--lazy-imports"))lazy_imports=true;
  else if(!strcmp(argv[i],"--target-triple")&&i+1<argc){
   const char *triple=argv[++i];size_t n=strlen(triple);
   if(!n||n>=256){fprintf(stderr,"swan: target triple must be 1-255 characters\n");return 2;}
   for(size_t k=0;k<n;k++)if(!((triple[k]>='A'&&triple[k]<='Z')||(triple[k]>='a'&&triple[k]<='z')||(triple[k]>='0'&&triple[k]<='9')||triple[k]=='-'||triple[k]=='_'||triple[k]=='.')){
    fprintf(stderr,"swan: invalid character in target triple\n");return 2;
   }
   custom_target.name="llvm";custom_target.triple=triple;custom_target.architecture=SWAN_ARCH_LLVM;
   custom_target.object_format=swan_object_format_for_triple(triple);custom_target.abi=SWAN_ABI_SYSV;
   custom_target.backend_available=true;target=&custom_target;
  }
  else if((!strcmp(argv[i],"--target-feature")||!strcmp(argv[i],"--wasm-feature"))&&i+1<argc){
   const char *feature=argv[++i];size_t n=strlen(feature);
   if(!n||n>=64||target_feature_count>=32){fprintf(stderr,"swan: invalid or excessive target feature\n");return 2;}
   for(size_t k=0;k<n;k++)if(!((feature[k]>='A'&&feature[k]<='Z')||(feature[k]>='a'&&feature[k]<='z')||(feature[k]>='0'&&feature[k]<='9')||feature[k]=='+'||feature[k]=='-'||feature[k]=='_')){
    fprintf(stderr,"swan: invalid target feature\n");return 2;
   }
   target_features[target_feature_count++]=feature;
  }
  else if(!strcmp(argv[i],"--cpu")&&i+1<argc)target_cpu=argv[++i];
  else if(!strcmp(argv[i],"--profile"))swan_profile_enabled=true;
  else if(!strcmp(argv[i],"--stdlib")&&i+1<argc)stdlib_dir=argv[++i];
  else if(!strcmp(argv[i],"-o")&&i+1<argc)output=argv[++i];
  else if(!strcmp(argv[i],"--link")&&i+1<argc){if(link_arg_count>=120){fprintf(stderr,"swan: too many linker inputs\n");return 2;}link_args[link_arg_count++]=argv[++i];}
  else if((!strcmp(argv[i],"-L")||!strcmp(argv[i],"-l"))&&i+1<argc){if(link_arg_count+2>120)return 2;link_args[link_arg_count++]=argv[i];link_args[link_arg_count++]=argv[++i];}
  else if((!strncmp(argv[i],"-L",2)||!strncmp(argv[i],"-l",2))&&argv[i][2]){if(link_arg_count>=120)return 2;link_args[link_arg_count++]=argv[i];}
  else if(!strcmp(argv[i],"--target")&&i+1<argc){
   const char *name=argv[++i];target=swan_target_find(name);
   if(!target){fprintf(stderr,"swan: unknown target '%s'; use --list-targets\n",name);return 2;}
   if(!target->backend_available){fprintf(stderr,"swan: target '%s' (%s) is recognized but its backend is not implemented yet\n",target->name,target->triple);return 2;}
  }else{fprintf(stderr,"swan: unknown option '%s'\n",argv[i]);return 2;}
 }
 if(link_arg_count&&target->architecture!=SWAN_ARCH_X86_64){fprintf(stderr,"swan: linker inputs are currently supported only by the native x86_64 executable path\n");return 2;}
 if((target_feature_count||target_cpu)&&target->architecture!=SWAN_ARCH_LLVM){fprintf(stderr,"swan: --cpu and --target-feature require --target-triple\n");return 2;}
 if(emit_llvm&&target->architecture!=SWAN_ARCH_LLVM){fprintf(stderr,"swan: --emit-llvm requires --target-triple\n");return 2;}
 if(emit_asm&&emit_llvm){fprintf(stderr,"swan: choose either --emit-asm or --emit-llvm\n");return 2;}
 if(emit_asm)mode=SWAN_EMIT;else if(emit_llvm)mode=SWAN_EMIT_LLVM;
 swan_profile_reset();
 uint64_t stage=swan_profile_start();
 char *source=swan_read_sources(swan_inputs,swan_input_count);
 swan_profile_end(SWAN_PROFILE_READ,stage);if(!source){perror(input);return 1;}
 if(c_input_count&&target->architecture!=SWAN_ARCH_X86_64){
  fprintf(stderr,"swanc: C source assembly bundling is currently supported only for native x86_64\n");free(source);return 2;
 }
 SwanError err={0};TokenStream ts;
 stage=swan_profile_start();
 if(!swan_lex(source,&ts,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.items);free(ts.text_storage);free(source);return 1;}
 swan_profile_end(SWAN_PROFILE_LEX,stage);
 Program p={0};p.codegen_module_index=-1;p.bundle_imports=!lazy_imports;
 stage=swan_profile_start();
 if(!swan_parse(&ts,&p,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.items);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 swan_profile_end(SWAN_PROFILE_PARSE,stage);
 free(ts.items);p.source_function_count=p.count;
 if(!swan_import_c_headers(&p,&err)){fprintf(stderr,"swan: %s\n",err.message);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 char *module_sources[64]={0},*module_texts[64]={0};size_t module_count=0;
 stage=swan_profile_start();
 if(!swan_load_imports(&p,stdlib_dir,module_sources,module_texts,&module_count,&err)){
  fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
 }
 swan_profile_end(SWAN_PROFILE_IMPORTS,stage);
 stage=swan_profile_start();
 if(!swan_semantic_check(&p,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 swan_profile_end(SWAN_PROFILE_SEMA,stage);
 stage=swan_profile_start();
 swan_compute_reachability(&p);swan_profile_end(SWAN_PROFILE_REACHABILITY,stage);
 if(target->architecture!=SWAN_ARCH_X86_64&&target->architecture!=SWAN_ARCH_LLVM){
  for(size_t i=0;i<p.count;i++)if(p.function_reachable[i]&&p.functions[i]->external){
   fprintf(stderr,"swan: external C ABI calls are currently supported only by the x86_64 System V backend\n");
   swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;
  }
 }
 bool has_lazy=swan_has_lazy_imports(&p);
 if(target->architecture==SWAN_ARCH_LLVM){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){
   fprintf(stderr,"swan: cross-target LLVM output cannot run on this host; use 'swan build' or --emit-asm\n");
   swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;
  }
  if(link_arg_count){fprintf(stderr,"swan: --link is not available with LLVM object emission yet\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  if(has_lazy){fprintf(stderr,"swan: the portable IR backend does not yet lower runtime-lazy imports\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  SwanSVMOutput compiled={0};
  bool generated=swan_svm_codegen(&p,target,&compiled,&err);
  Text llvm={.data=compiled.data,.len=compiled.length};
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"SVM backend failed");free(llvm.data);
   swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *suffix=target->object_format==SWAN_OBJECT_COFF?".obj":".o";
  char default_output[64];snprintf(default_output,sizeof(default_output),"a%s",mode==SWAN_EMIT_LLVM?".ll":mode==SWAN_EMIT?".s":suffix);
  const char *dest=output?output:default_output;int result=0;
  if(mode==SWAN_EMIT_LLVM){
   FILE *of=fopen(dest,"wb");bool ok=of&&fwrite(llvm.data,1,llvm.len,of)==llvm.len;
   if(of&&fclose(of)!=0)ok=false;
    if(!ok){fprintf(stderr,"swan: cannot write LLVM IR '%s'\n",dest);result=1;}
  }else{
   char temp[]="/tmp/swan-llvm-XXXXXX";int fd=mkstemp(temp);
   if(fd<0)result=1;
   else{
    size_t written=0;while(written<llvm.len){ssize_t n=write(fd,llvm.data+written,llvm.len-written);if(n<0){if(errno==EINTR)continue;result=1;break;}written+=(size_t)n;}
    if(close(fd)!=0)result=1;
    if(!result){
     char *args[180];size_t n=0;args[n++]="clang";
     char target_arg[280];snprintf(target_arg,sizeof(target_arg),"--target=%s",target->triple);args[n++]=target_arg;
     char cpu_arg[160];if(target_cpu){snprintf(cpu_arg,sizeof(cpu_arg),"-mcpu=%s",target_cpu);args[n++]=cpu_arg;}
     for(size_t k=0;k<target_feature_count;k++){args[n++]="-Xclang";args[n++]="-target-feature";args[n++]="-Xclang";args[n++]=(char*)target_features[k];}
     args[n++]=mode==SWAN_EMIT?"-S":"-c";args[n++]="-x";args[n++]="ir";args[n++]=temp;args[n++]="-o";args[n++]=(char*)dest;args[n]=NULL;
     result=swan_exec(args,true);
    }
    unlink(temp);
   }
   if(result)fprintf(stderr,"swan: Clang could not emit target output for '%s'; check that this Clang build includes the requested target backend\n",target->triple);
  }
  free(llvm.data);swan_release_modules(module_sources,module_texts,module_count);
  free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_WASM32||target->architecture==SWAN_ARCH_WASM64){
  if(c_input_count){fprintf(stderr,"swan: portable WebAssembly output currently accepts Swan sources only; C translation units require the native x86-64 linker path\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  if(mode!=SWAN_BUILD&&mode!=SWAN_EMIT){fprintf(stderr,"swan: WASM32 output is a WebAssembly module; run it with a WebAssembly runtime\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  if(has_lazy){fprintf(stderr,"swan: WASM32 backend does not yet support runtime-lazy imports\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  SwanSVMOutput compiled={0};
  bool backend_ok=swan_svm_codegen(&p,target,&compiled,&err);
  SwanWasmBuffer wasm={.data=(unsigned char*)compiled.data,.length=compiled.length,.capacity=compiled.length};
  if(!backend_ok){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"SVM backend failed");
   free(wasm.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(target->architecture==SWAN_ARCH_WASM64?"a64.wasm":"a.wasm");FILE *wf=fopen(dest,"wb");
  bool saved=wf&&fwrite(wasm.data,1,wasm.length,wf)==wasm.length;
  if(wf&&fclose(wf)!=0)saved=false;
  if(!saved)fprintf(stderr,"swan: failed to write WebAssembly module '%s'\n",dest);
  free(wasm.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return saved?0:1;
 }
 if(target->architecture==SWAN_ARCH_AARCH64){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){fprintf(stderr,"swan: AArch64 cross-target output is an ELF object; execute it on an AArch64 system after linking\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}

  SwanSVMOutput compiled={0};
  bool generated=swan_svm_codegen(&p,target,&compiled,&err);
  Text assembly={.data=compiled.data,.len=compiled.length};
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"SVM backend failed");free(assembly.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"aarch64.s":"aarch64.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-a64-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=aarch64-unknown-linux-gnu","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: AArch64 assembly failed; install a Clang toolchain with AArch64 support\n");
  }
  free(assembly.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_ARMV7){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){fprintf(stderr,"swan: ARMv7 cross-target output is an ELF object; execute it on an ARMv7 system after linking\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}

  SwanSVMOutput compiled={0};
  bool generated=swan_svm_codegen(&p,target,&compiled,&err);
  Text assembly={.data=compiled.data,.len=compiled.length};
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"SVM backend failed");free(assembly.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"armv7.s":"armv7.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-armv7-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=armv7-unknown-linux-gnueabihf","-marm","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: ARMv7 assembly failed; install a Clang toolchain with ARM support\n");
  }
  free(assembly.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_RISCV64){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){fprintf(stderr,"swan: RISC-V64 cross-target output is an ELF object; execute it on a RISC-V64 system after linking\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}

  SwanSVMOutput compiled={0};
  bool generated=swan_svm_codegen(&p,target,&compiled,&err);
  Text assembly={.data=compiled.data,.len=compiled.length};
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"SVM backend failed");free(assembly.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"riscv64.s":"riscv64.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-rv64-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=riscv64-unknown-linux-gnu","-march=rv64gc","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: RISC-V64 assembly failed; install a Clang toolchain with RISC-V support\n");
  }
  free(assembly.data);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(mode==SWAN_EMIT&&has_lazy){fprintf(stderr,"swan: --emit-asm cannot bundle runtime-lazy modules; use 'swan build' or 'swan run'\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
 char exe[1024];bool temporary_output=output==NULL;int exe_len;
 if(output)exe_len=snprintf(exe,sizeof(exe),"%s",output);
 else if(mode==SWAN_BUILD)exe_len=snprintf(exe,sizeof(exe),"a.out");
 else exe_len=snprintf(exe,sizeof(exe),"/tmp/swan-run-%ld",(long)getpid());
 if(exe_len<0||(size_t)exe_len>=sizeof(exe)){fprintf(stderr,"swan: output path is too long\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 SwanSVMOutput compiled={0};
 if(c_input_count){
  /* Keep mixed Swan/C translation units on the shared assembler syntax path;
     appended C assembly currently uses Intel syntax from the host driver. */
  Text native={0};
  if(!swan_codegen(&p,&native,target,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(native.data);return 1;}
  compiled.kind=SWAN_SVM_OUTPUT_ASSEMBLY;compiled.data=native.data;compiled.length=native.len;
 }else if(!swan_svm_codegen(&p,target,&compiled,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(compiled.data);return 1;}
 Text assembly={0};
 if(compiled.kind==SWAN_SVM_OUTPUT_LLVM_IR){
  bool lowered=swan_llvm_ir_to_assembly(compiled.data,compiled.length,&assembly,&err);
  free(compiled.data);compiled.data=NULL;
  if(!lowered){fprintf(stderr,"swan: %s\n",err.message[0]?err.message:"native LLVM code generation failed");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 }else assembly=(Text){.data=compiled.data,.len=compiled.length};
 if(has_lazy&&!swan_build_lazy_modules(&p,exe,&err)){fprintf(stderr,"swan: %s\n",err.message);swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 char runtime_source[64]={0};if(has_lazy&&!swan_write_lazy_runtime(runtime_source)){fprintf(stderr,"swan: cannot create lazy import runtime support\n");swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 if(mode==SWAN_EMIT){
  const char *dest=output?output:"a.s";FILE *of=fopen(dest,"wb");
  if(!of){perror(dest);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  stage=swan_profile_start();bool ok=swan_emit_assembly(of,&assembly);
  if(ok&&c_input_count)ok=swan_append_c_assembly(of,c_inputs,c_input_count);
  if(fclose(of)!=0)ok=false;
  swan_profile_end(SWAN_PROFILE_ASM_WRITE,stage);
  if(!ok){fprintf(stderr,"swan: failed to write assembly output '%s'\n",dest);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  if(runtime_source[0])unlink(runtime_source);
  swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 0;
 }
 char temp[]="/tmp/swan-XXXXXX";int fd=mkstemp(temp);
 if(fd<0){perror("mkstemp");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 FILE *af=fdopen(fd,"wb");
 if(!af){close(fd);unlink(temp);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 stage=swan_profile_start();bool written=swan_emit_assembly(af,&assembly);
 if(written&&c_input_count)written=swan_append_c_assembly(af,c_inputs,c_input_count);
 if(fclose(af)!=0)written=false;
 swan_profile_end(SWAN_PROFILE_ASM_WRITE,stage);
 if(!written){fprintf(stderr,"swan: failed to write temporary assembly\n");unlink(temp);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 int result=0;
 if(has_lazy){
  char *cc_args[136];size_t cc_count=0;
  cc_args[cc_count++]="cc";cc_args[cc_count++]="-rdynamic";cc_args[cc_count++]="-x";cc_args[cc_count++]="assembler";cc_args[cc_count++]=temp;
  cc_args[cc_count++]="-x";cc_args[cc_count++]="c";cc_args[cc_count++]=runtime_source;cc_args[cc_count++]="-ldl";cc_args[cc_count++]="-x";cc_args[cc_count++]="none";
  for(size_t i=0;i<link_arg_count;i++)cc_args[cc_count++]=(char*)link_args[i];
  cc_args[cc_count++]="-o";cc_args[cc_count++]=exe;cc_args[cc_count]=NULL;
  result=swan_exec(cc_args,true);unlink(temp);unlink(runtime_source);
  if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);if(temporary_output)swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 }else{
  result=swan_link_assembly_with_args("cc",temp,exe,link_args,link_arg_count);unlink(temp);
  if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 }
 if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 if(mode==SWAN_BUILD){swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 0;}
 char *run_args[]={exe,NULL};stage=swan_profile_start();result=swan_exec(run_args,false);swan_profile_end(SWAN_PROFILE_RUN,stage);
 if(temporary_output){unlink(exe);if(has_lazy)swan_remove_lazy_modules(exe,&p);}
 swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);
 return result<0?1:result;
}

int main(int argc,char **argv){int result=swan_run(argc,argv);swan_profile_print();return result;}
