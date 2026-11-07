#ifndef SWAN_COMPILER_H
#define SWAN_COMPILER_H
#include "swan/codegen_x86_64.h"
#include "swan/sema.h"
#include <unistd.h>
static inline char *swan_read_file(const char *path){
 FILE *f=fopen(path,"rb");if(!f)return NULL;
 if(fseek(f,0,SEEK_END)){fclose(f);return NULL;}long n=ftell(f);
 if(n<0){fclose(f);return NULL;}rewind(f);
 char *s=malloc((size_t)n+1);if(!s){fclose(f);return NULL;}
 size_t got=fread(s,1,(size_t)n,f);fclose(f);s[got]=0;return s;
}
static inline void swan_shell_quote(char *out,size_t cap,const char *in){
 size_t n=0;if(cap<3){if(cap)out[0]=0;return;}out[n++]='\'';
 for(;*in&&n+5<cap;in++){if(*in=='\''){out[n++]='\'';out[n++]='\\';out[n++]='\'';out[n++]='\'';}else out[n++]=*in;}
 out[n++]='\'';out[n]=0;
}
static inline int swan_run(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--version")){puts("Swan " SWAN_VERSION);return 0;}
 if(argc==2&&(!strcmp(argv[1],"--help")||!strcmp(argv[1],"-h"))){printf("Swan %s\nUsage: swan <file.swan> [--emit-asm] [-o output]\n",SWAN_VERSION);return 0;}
 if(argc<2){fprintf(stderr,"Swan %s\nUsage: swan <file.swan> [--emit-asm] [-o output]\n",SWAN_VERSION);return 2;}
 const char *input=argv[1],*output=NULL;bool emit_asm=false;
 for(int i=2;i<argc;i++){
  if(!strcmp(argv[i],"--emit-asm"))emit_asm=true;
  else if(!strcmp(argv[i],"-o")&&i+1<argc)output=argv[++i];
  else{fprintf(stderr,"swan: unknown option '%s'\n",argv[i]);return 2;}
 }
 char *source=swan_read_file(input);if(!source){perror(input);return 1;}
 SwanError err={0};TokenStream ts;
 if(!swan_lex(source,&ts,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.items);free(ts.text_storage);free(source);return 1;}
 Program p={0};
 if(!swan_parse(&ts,&p,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);free(ts.items);free(ts.text_storage);swan_program_destroy(&p);free(source);return 1;}
 free(ts.items);free(ts.text_storage);
 if(!swan_semantic_check(&p,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_program_destroy(&p);free(source);return 1;}
 Text assembly={0};
 if(!swan_codegen_x86_64(&p,&assembly,&err)){fprintf(stderr,"swan:%u:%u: %s\n",err.line,err.col,err.message);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 if(emit_asm){
  const char *dest=output?output:"a.s";FILE *of=fopen(dest,"wb");
  if(!of){perror(dest);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
  fwrite(assembly.data,1,assembly.len,of);fclose(of);swan_program_destroy(&p);free(source);free(assembly.data);return 0;
 }
 char temp[]="/tmp/swan-XXXXXX";int fd=mkstemp(temp);
 if(fd<0){perror("mkstemp");swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 FILE *af=fdopen(fd,"wb");if(!af){close(fd);unlink(temp);swan_program_destroy(&p);free(source);free(assembly.data);return 1;}
 fwrite(assembly.data,1,assembly.len,af);fclose(af);
 char exe[1024];if(output)snprintf(exe,sizeof(exe),"%s",output);else snprintf(exe,sizeof(exe),"/tmp/swan-run-%ld",(long)getpid());
 char qtemp[1200],qexe[1200],command[4096];swan_shell_quote(qtemp,sizeof(qtemp),temp);swan_shell_quote(qexe,sizeof(qexe),exe);
 snprintf(command,sizeof(command),"cc -x assembler %s -o %s && %s",qtemp,qexe,qexe);
 int result=system(command);unlink(temp);if(!output)unlink(exe);
 free(source);swan_program_destroy(&p);free(assembly.data);return result==0?0:1;
}
#endif
