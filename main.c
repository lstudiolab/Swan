#define _POSIX_C_SOURCE 200809L
#include "swan/compiler.h"

/* The executable entry point owns command-line orchestration. The headers
 * provide independently testable compiler stages and shared utilities. */
static int swan_run(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--version")){puts("Swan " SWAN_VERSION);return 0;}
 if(argc==2&&!strcmp(argv[1],"--list-targets")){swan_target_list(stdout);return 0;}
 if(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){printf("Swan %s\nUsage: swan [build|run] <file.swan> [--emit-asm] [--target <target>] [--list-targets] [--stdlib directory] [-o output]\n",SWAN_VERSION);return 0;}
 enum { SWAN_RUN_DEFAULT,SWAN_BUILD,SWAN_RUN,SWAN_EMIT } mode=SWAN_RUN_DEFAULT;
 int index=1;
 if(index<argc&&!strcmp(argv[index],"build")){mode=SWAN_BUILD;index++;}
 else if(index<argc&&!strcmp(argv[index],"run")){mode=SWAN_RUN;index++;}
 if(index>=argc){fprintf(stderr,"Swan %s\nUsage: swan [build|run] <file.swan> [--emit-asm] [--target x86_64] [--stdlib directory] [-o output]\n",SWAN_VERSION);return 2;}
 const char *input=argv[index++],*output=NULL,*stdlib_dir=getenv("SWAN_STDLIB");
 if(!stdlib_dir||!*stdlib_dir)stdlib_dir="stdlib";
 bool emit_asm=false;const SwanTarget *target=swan_target_find("x86_64");
 for(int i=index;i<argc;i++){
  if(!strcmp(argv[i],"--emit-asm"))emit_asm=true;
  else if(!strcmp(argv[i],"--stdlib")&&i+1<argc)stdlib_dir=argv[++i];
  else if(!strcmp(argv[i],"-o")&&i+1<argc)output=argv[++i];
  else if(!strcmp(argv[i],"--target")&&i+1<argc){
   const char *name=argv[++i];target=swan_target_find(name);
   if(!target){fprintf(stderr,"swan: unknown target '%s'; use --list-targets\n",name);return 2;}
   if(!target->backend_available){fprintf(stderr,"swan: target '%s' (%s) is recognized but its backend is not implemented yet\n",target->name,target->triple);return 2;}
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
 if(target->architecture==SWAN_ARCH_WASM32||target->architecture==SWAN_ARCH_WASM64){
  if(mode!=SWAN_BUILD&&mode!=SWAN_EMIT){fprintf(stderr,"swan: WASM32 output is a WebAssembly module; run it with a WebAssembly runtime\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  if(has_lazy){fprintf(stderr,"swan: WASM32 backend does not yet support runtime-lazy imports\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  SwanIRModule ir={0};char detail[256]={0};SwanWasmBuffer wasm={0};
  if(!swan_irgen_program(&p,&ir,&err)||!swan_ir_optimize(&ir,detail,sizeof(detail))||!(target->architecture==SWAN_ARCH_WASM64?swan_codegen_wasm64(&ir,&wasm,detail,sizeof(detail)):swan_codegen_wasm32(&ir,&wasm,detail,sizeof(detail)))){
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
  if(has_lazy){fprintf(stderr,"swan: AArch64 object backend does not yet support runtime-lazy imports\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  SwanIRModule ir={0};Text assembly={0};char detail[256]={0};
  if(!swan_irgen_program(&p,&ir,&err)||!swan_ir_optimize(&ir,detail,sizeof(detail))||!swan_codegen_aarch64_ir(&ir,&assembly,detail,sizeof(detail))){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:detail);free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"aarch64.s":"aarch64.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&fwrite(assembly.data,1,assembly.len,of)==assembly.len;if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-a64-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&fwrite(assembly.data,1,assembly.len,of)==assembly.len;if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=aarch64-unknown-linux-gnu","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: AArch64 assembly failed; install a Clang toolchain with AArch64 support\n");
  }
  free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_RISCV64){
  if(mode==SWAN_RUN||mode==SWAN_RUN_DEFAULT){fprintf(stderr,"swan: RISC-V64 cross-target output is an ELF object; execute it on a RISC-V64 system after linking\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  if(has_lazy){fprintf(stderr,"swan: RISC-V64 backend does not yet support runtime-lazy imports\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  SwanIRModule ir={0};Text assembly={0};char detail[256]={0};
  if(!swan_irgen_program(&p,&ir,&err)||!swan_ir_optimize(&ir,detail,sizeof(detail))||!swan_codegen_riscv64_ir(&ir,&assembly,detail,sizeof(detail))){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:detail);free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  const char *dest=output?output:(mode==SWAN_EMIT?"riscv64.s":"riscv64.o");int result=0;
  if(mode==SWAN_EMIT){FILE *of=fopen(dest,"wb");bool ok=of&&fwrite(assembly.data,1,assembly.len,of)==assembly.len;if(of&&fclose(of)!=0)ok=false;if(!ok){fprintf(stderr,"swan: cannot write assembly '%s'\n",dest);result=1;}}
  else {
   char temp[]="/tmp/swan-rv64-XXXXXX";int fd=mkstemp(temp);if(fd<0)result=1;
   else {FILE *of=fdopen(fd,"wb");bool ok=of&&fwrite(assembly.data,1,assembly.len,of)==assembly.len;if(of&&fclose(of)!=0)ok=false;if(!ok){if(!of)close(fd);result=1;}else{char *args[]={"clang","--target=riscv64-unknown-linux-gnu","-march=rv64gc","-c","-x","assembler",temp,"-o",(char*)dest,NULL};result=swan_exec(args,true);}unlink(temp);}
   if(result)fprintf(stderr,"swan: RISC-V64 assembly failed; install a Clang toolchain with RISC-V support\n");
  }
  free(assembly.data);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return result;
 }
 if(target->architecture==SWAN_ARCH_SVM){
  if(mode==SWAN_EMIT){fprintf(stderr,"swan: --emit-asm is not valid for the SVM target\n");swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 2;}
  SwanIRModule ir={0};char vm_error[256]={0};
  if(!swan_irgen_program(&p,&ir,&err)||!swan_ir_optimize(&ir,vm_error,sizeof(vm_error))){
   fprintf(stderr,"swan: %s\n",err.message[0]?err.message:vm_error);
   swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;
  }
  char module_path[1024];bool temporary=output==NULL&&mode!=SWAN_BUILD;
  const char *dest=output?output:(mode==SWAN_BUILD?"a.swvm":"/tmp/swan-module-XXXXXX");
  int fd=-1;
  if(temporary){snprintf(module_path,sizeof(module_path),"/tmp/swan-module-XXXXXX");fd=mkstemp(module_path);if(fd>=0)close(fd);dest=module_path;}
  FILE *artifact=fd>=0||!temporary?fopen(dest,"wb"):NULL;
  bool saved=artifact&&swan_svm_write_module(artifact,&ir);
  if(artifact&&fclose(artifact)!=0)saved=false;
  if(!saved){fprintf(stderr,"swan: failed to write portable SVM module\n");if(temporary)unlink(dest);swan_ir_destroy(&ir);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
  int result=0;
  if(mode!=SWAN_BUILD){int64_t vm_result=0;if(!swan_svm_run_file(dest,"main",UINT64_C(1000000000),&vm_result,vm_error,sizeof(vm_error))){fprintf(stderr,"swan: %s\n",vm_error);result=1;}else result=(int)(uint8_t)vm_result;}
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
 int result=0;
 if(has_lazy){
  char *cc_args[]={"cc","-rdynamic","-x","assembler",temp,"-x","c",runtime_source,"-ldl","-o",exe,NULL};
  result=swan_exec(cc_args,true);unlink(temp);unlink(runtime_source);
  if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);if(temporary_output)swan_remove_lazy_modules(exe,&p);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 }else{
  char *cc_args[]={"cc","-x","assembler",temp,"-o",exe,NULL};
  result=swan_exec(cc_args,true);unlink(temp);
  if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 }
 if(result!=0){if(temporary_output&&mode!=SWAN_BUILD)unlink(exe);swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return result<0?1:result;}
 if(mode==SWAN_BUILD){swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);return 0;}
 char *run_args[]={exe,NULL};result=swan_exec(run_args,false);
 if(temporary_output){unlink(exe);if(has_lazy)swan_remove_lazy_modules(exe,&p);}
 swan_release_modules(module_sources,module_texts,module_count);free(ts.text_storage);swan_program_destroy(&p);free(source);free(assembly.data);
 return result<0?1:result;
}

int main(int argc,char **argv){return swan_run(argc,argv);}
