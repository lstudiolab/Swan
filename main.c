#define _POSIX_C_SOURCE 200809L
#include "swan/compiler.h"

/* The executable entry point owns command-line orchestration. The headers
 * provide independently testable compiler stages and shared utilities. */
static int swan_run(int argc,char **argv){
 if(argc>=3&&!strcmp(argv[1],"vm")){
  uint64_t fuel=10000000u;
  if(argc==5&&!strcmp(argv[3],"--fuel")){char *end=NULL;errno=0;unsigned long long parsed=strtoull(argv[4],&end,10);if(errno||!end||*end||!parsed){fprintf(stderr,"swan: invalid SVM fuel limit '%s'\\n",argv[4]);return 2;}fuel=(uint64_t)parsed;}
  else if(argc!=3){fprintf(stderr,"Usage: swan vm <file.swvm> [--fuel instructions]\\n");return 2;}
  FILE *artifact=fopen(argv[2],"rb");if(!artifact){perror(argv[2]);return 1;}
  SwanIRModule module={0};char detail[256]={0};
  bool loaded=swan_svm_read_module(artifact,&module,detail,sizeof(detail));fclose(artifact);
  if(!loaded){fprintf(stderr,"swan: invalid SVM module: %s\\n",detail);swan_ir_destroy(&module);return 1;}
  int64_t result=0;bool ok=swan_svm_run(&module,"main",fuel,&result,detail,sizeof(detail));
  if(!ok)fprintf(stderr,"swan: SVM execution failed: %s\\n",detail);
  swan_ir_destroy(&module);return ok?(int)((uint64_t)result&255u):1;
 }
 if(argc==2&&!strcmp(argv[1],"--version")){puts("Swan " SWAN_VERSION);return 0;}
 if(argc==2&&!strcmp(argv[1],"--list-targets")){swan_target_list(stdout);return 0;}
 if(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){printf("Swan %s\nUsage: swan [build|run] <file.swan> [--target <target>] [--link object] [-L dir] [-l library] [-o output] | swan vm <file.swvm> [--fuel instructions]\n",SWAN_VERSION);return 0;}
 enum { SWAN_RUN_DEFAULT,SWAN_BUILD,SWAN_RUN,SWAN_EMIT,SWAN_EMIT_LLVM } mode=SWAN_RUN_DEFAULT;
 int index=1;
 if(index<argc&&!strcmp(argv[index],"build")){mode=SWAN_BUILD;index++;}
 else if(index<argc&&!strcmp(argv[index],"run")){mode=SWAN_RUN;index++;}
 if(index<argc&&!strcmp(argv[index],"--profile")){swan_profile_enabled=true;index++;}
 if(index>=argc){fprintf(stderr,"Swan %s\nUsage: swan [build|run] <file.swan> [--emit-asm] [--target x86_64] [--stdlib directory] [-o output]\n",SWAN_VERSION);return 2;}
 const char *input=argv[index++],*output=NULL,*stdlib_dir=getenv("SWAN_STDLIB");
 if(!stdlib_dir||!*stdlib_dir)stdlib_dir="stdlib";
 bool emit_asm=false,emit_llvm=false;const char *link_args[120];size_t link_arg_count=0;
 const char *target_features[32];size_t target_feature_count=0;const char *target_cpu=NULL;
 SwanTarget custom_target={0};const SwanTarget *target=swan_target_find("x86_64");
 for(int i=index;i<argc;i++){
  if(!strcmp(argv[i],"--emit-asm"))emit_asm=true;
  else if(!strcmp(argv[i],"--emit-llvm"))emit_llvm=true;
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
  else if(!strcmp(argv[i],"--target-feature")&&i+1<argc){
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
 if(emit_asm&&emit_llvm){fprintf(stderr,"swan: choose either --emit-asm or --emit-llvm\n");return 2;}
 if(emit_asm)mode=SWAN_EMIT;else if(emit_llvm)mode=SWAN_EMIT_LLVM;
 swan_profile_reset();
 uint64_t stage=swan_profile_start();
 char *source=swan_read_file(input);
 swan_profile_end(SWAN_PROFILE_READ,stage);if(!source){perror(input);return 1;}
 SwanError err={0};TokenStream ts;
 stage=swan_profile_start();
 if(!swan_lex(source,&ts,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.items);free(ts.text_storage);free(source);return 1;}
 swan_profile_end(SWAN_PROFILE_LEX,stage);
 Program p={0};p.codegen_module_index=-1;
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
  SwanIRModule ir={0};Text llvm={0};char detail[256]={0};
  stage=swan_profile_start();bool lowered=swan_compile_ir_program(&p,&ir,&err);swan_profile_end(SWAN_PROFILE_IRGEN,stage);
  stage=swan_profile_start();bool optimized=lowered&&swan_ir_optimize(&ir,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_IR_OPT,stage);
  stage=swan_profile_start();bool generated=optimized&&swan_codegen_llvm_ir(&ir,&llvm,target->triple,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_CODEGEN,stage);
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:detail);free(llvm.data);swan_ir_destroy(&ir);
   swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *suffix=target->object_format==SWAN_OBJECT_COFF?".obj":".o";
  char default_output[64];snprintf(default_output,sizeof(default_output),"a%s",mode==SWAN_EMIT_LLVM?".ll":mode==SWAN_EMIT?".s":suffix);
  const char *dest=output?output:default_output;int result=0;
  if(mode==SWAN_EMIT_LLVM){
   FILE *of=fopen(dest,"wb");bool ok=of&&fwrite(llvm.data,1,llvm.len,of)==llvm.len;
   if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write LLVM IR '%s'\n",dest);result=1;}
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
  free(llvm.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);
  free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_WASM32||target->architecture==SWAN_ARCH_WASM64){
  if(mode!=SWAN_BUILD&&mode!=SWAN_EMIT){fprintf(stderr,"swan: WASM32 output is a WebAssembly module; run it with a WebAssembly runtime\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  if(has_lazy){fprintf(stderr,"swan: WASM32 backend does not yet support runtime-lazy imports\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  SwanIRModule ir={0};char detail[256]={0};SwanWasmBuffer wasm={0};
  stage=swan_profile_start();bool ir_ok=swan_compile_ir_program(&p,&ir,&err);swan_profile_end(SWAN_PROFILE_IRGEN,stage);
   stage=swan_profile_start();bool opt_ok=ir_ok&&swan_ir_optimize(&ir,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_IR_OPT,stage);
   stage=swan_profile_start();bool backend_ok=opt_ok&&(target->architecture==SWAN_ARCH_WASM64?swan_codegen_wasm64(&ir,&wasm,detail,sizeof(detail)):swan_codegen_wasm32(&ir,&wasm,detail,sizeof(detail)));swan_profile_end(SWAN_PROFILE_CODEGEN,stage);
  if(!backend_ok){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:detail);
   free(wasm.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(target->architecture==SWAN_ARCH_WASM64?"a64.wasm":"a.wasm");FILE *wf=fopen(dest,"wb");
  bool saved=wf&&fwrite(wasm.data,1,wasm.length,wf)==wasm.length;
  if(wf&&fclose(wf)!=0)saved=false;
  if(!saved)fprintf(stderr,"swan: failed to write WebAssembly module '%s'\n",dest);
  free(wasm.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return saved?0:1;
 }
 if(target->architecture==SWAN_ARCH_AARCH64){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){fprintf(stderr,"swan: AArch64 cross-target output is an ELF object; execute it on an AArch64 system after linking\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}

  SwanIRModule ir={0};Text assembly={0};char detail[256]={0};
  stage=swan_profile_start();bool lowered=swan_compile_ir_program(&p,&ir,&err);swan_profile_end(SWAN_PROFILE_IRGEN,stage);stage=swan_profile_start();bool optimized=lowered&&swan_ir_optimize(&ir,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_IR_OPT,stage);stage=swan_profile_start();bool generated=optimized&&swan_codegen_aarch64_ir(&ir,&assembly,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_CODEGEN,stage);
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:detail);free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"aarch64.s":"aarch64.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-a64-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=aarch64-unknown-linux-gnu","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: AArch64 assembly failed; install a Clang toolchain with AArch64 support\n");
  }
  free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_ARMV7){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){fprintf(stderr,"swan: ARMv7 cross-target output is an ELF object; execute it on an ARMv7 system after linking\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}

  SwanIRModule ir={0};Text assembly={0};char detail[256]={0};
  stage=swan_profile_start();bool lowered=swan_compile_ir_program(&p,&ir,&err);swan_profile_end(SWAN_PROFILE_IRGEN,stage);stage=swan_profile_start();bool optimized=lowered&&swan_ir_optimize(&ir,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_IR_OPT,stage);stage=swan_profile_start();bool generated=optimized&&swan_codegen_armv7_ir(&ir,&assembly,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_CODEGEN,stage);
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:detail);free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"armv7.s":"armv7.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-armv7-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=armv7-unknown-linux-gnueabihf","-marm","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: ARMv7 assembly failed; install a Clang toolchain with ARM support\n");
  }
  free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_RISCV64){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){fprintf(stderr,"swan: RISC-V64 cross-target output is an ELF object; execute it on a RISC-V64 system after linking\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}

  SwanIRModule ir={0};Text assembly={0};char detail[256]={0};
  stage=swan_profile_start();bool lowered=swan_compile_ir_program(&p,&ir,&err);swan_profile_end(SWAN_PROFILE_IRGEN,stage);stage=swan_profile_start();bool optimized=lowered&&swan_ir_optimize(&ir,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_IR_OPT,stage);stage=swan_profile_start();bool generated=optimized&&swan_codegen_riscv64_ir(&ir,&assembly,detail,sizeof(detail));swan_profile_end(SWAN_PROFILE_CODEGEN,stage);
  if(!generated){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:detail);free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"riscv64.s":"riscv64.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-rv64-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&swan_emit_assembly(of,&assembly);if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=riscv64-unknown-linux-gnu","-march=rv64gc","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: RISC-V64 assembly failed; install a Clang toolchain with RISC-V support\n");
  }
  free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_SVM){
  if(mode==SWAN_EMIT){fprintf(stderr,"swan: --emit-asm is not valid for the SVM target\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  SwanIRModule ir={0};char vm_error[256]={0};
  stage=swan_profile_start();bool lowered=swan_compile_ir_program(&p,&ir,&err);swan_profile_end(SWAN_PROFILE_IRGEN,stage);stage=swan_profile_start();bool optimized=lowered&&swan_ir_optimize(&ir,vm_error,sizeof(vm_error));swan_profile_end(SWAN_PROFILE_IR_OPT,stage);
  if(!optimized){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:vm_error);
   swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  char module_path[1024];bool temporary=output==NULL&&mode!=SWAN_BUILD;
  const char *dest=output?output:(mode==SWAN_BUILD?"a.swvm":"/tmp/swan-module-XXXXXX");
  int fd=-1;
  if(temporary){snprintf(module_path,sizeof(module_path),"/tmp/swan-module-XXXXXX");fd=mkstemp(module_path);if(fd>=0)close(fd);dest=module_path;}
  FILE *artifact=fd>=0||!temporary?fopen(dest,"wb"):NULL;
  stage=swan_profile_start();bool saved=artifact&&swan_svm_write_module(artifact,&ir);swan_profile_end(SWAN_PROFILE_ASM_WRITE,stage);
  if(artifact&&fclose(artifact)!=0)saved=false;
  if(!saved){fprintf(stderr,"swan: failed to write portable SVM module\n");if(temporary)unlink(dest);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
  int result=0;
  if(mode!=SWAN_BUILD){int64_t vm_result=0;stage=swan_profile_start();if(!swan_svm_run_file(dest,"main",UINT64_C(1000000000),&vm_result,vm_error,sizeof(vm_error))){fprintf(stderr,"swan: %s\n",vm_error);result=1;}else result=(int)(uint8_t)vm_result;swan_profile_end(SWAN_PROFILE_RUN,stage);}
  if(temporary)unlink(dest);
  swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(mode==SWAN_EMIT&&has_lazy){fprintf(stderr,"swan: --emit-asm cannot bundle runtime-lazy modules; use 'swan build' or 'swan run'\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
 char exe[1024];bool temporary_output=output==NULL;int exe_len;
 if(output)exe_len=snprintf(exe,sizeof(exe),"%s",output);
 else if(mode==SWAN_BUILD)exe_len=snprintf(exe,sizeof(exe),"a.out");
 else exe_len=snprintf(exe,sizeof(exe),"/tmp/swan-run-%ld",(long)getpid());
 if(exe_len<0||(size_t)exe_len>=sizeof(exe)){fprintf(stderr,"swan: output path is too long\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 Text assembly={0};
 if(!swan_codegen(&p,&assembly,target,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 if(has_lazy&&!swan_build_lazy_modules(&p,exe,&err)){fprintf(stderr,"swan: %s\n",err.message);swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 char runtime_source[64]={0};if(has_lazy&&!swan_write_lazy_runtime(runtime_source)){fprintf(stderr,"swan: cannot create lazy import runtime support\n");swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 if(mode==SWAN_EMIT){
  const char *dest=output?output:"a.s";FILE *of=fopen(dest,"wb");
  if(!of){perror(dest);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  stage=swan_profile_start();bool ok=swan_emit_assembly(of,&assembly);if(fclose(of)!=0)ok=false;swan_profile_end(SWAN_PROFILE_ASM_WRITE,stage);
  if(!ok){fprintf(stderr,"swan: failed to write assembly output '%s'\n",dest);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  if(runtime_source[0])unlink(runtime_source);
  swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 0;
 }
 char temp[]="/tmp/swan-XXXXXX";int fd=mkstemp(temp);
 if(fd<0){perror("mkstemp");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 FILE *af=fdopen(fd,"wb");
 if(!af){close(fd);unlink(temp);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 stage=swan_profile_start();bool written=swan_emit_assembly(af,&assembly);
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
