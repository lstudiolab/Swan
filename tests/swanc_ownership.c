#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern int64_t swan_fn_swanc_ownership_analyze(uint32_t*,int64_t,int64_t,uint32_t*,int64_t);
enum { DECLARE=1, READ, WRITE, BORROW_SHARED, BORROW_MUT, END_SHARED, END_MUT, MOVE, DROP };
static void ev(uint32_t *e,uint32_t i,uint32_t k,uint32_t s,uint32_t loc){uint32_t p=i*4;e[p]=k;e[p+1]=s;e[p+2]=loc;e[p+3]=0;}
static int bad(uint32_t *e,uint32_t n,uint32_t symbols,uint32_t code,const char *name){
 uint32_t w[36]={0};int64_t r=swan_fn_swanc_ownership_analyze(e,n,symbols,w,36);
 if(r==0||w[0]!=code){fprintf(stderr,"%s: status=%lld diagnostic=%u expected=%u\n",name,(long long)r,w[0],code);return 1;}return 0;
}
static int valid(void){
 uint32_t e[12*4]={0},w[16]={0};
 ev(e,0,DECLARE,0,10);ev(e,1,READ,0,11);ev(e,2,BORROW_SHARED,0,12);
 ev(e,3,READ,0,13);ev(e,4,BORROW_SHARED,0,14);ev(e,5,END_SHARED,0,15);
 ev(e,6,END_SHARED,0,16);ev(e,7,BORROW_MUT,0,17);ev(e,8,WRITE,0,18);
 ev(e,9,END_MUT,0,19);ev(e,10,MOVE,0,20);ev(e,11,DECLARE,1,21);
 if(swan_fn_swanc_ownership_analyze(e,12,3,w,16)!=0){fprintf(stderr,"valid flow rejected: %u at %u\n",w[0],w[1]);return 1;}
 if(w[4]!=2||w[8]!=1||w[12]!=0)return 2;
 return 0;
}
static int negative(void){
 uint32_t e[4*4]={0};
 ev(e,0,DECLARE,0,1);ev(e,1,MOVE,0,2);ev(e,2,READ,0,3);if(bad(e,3,1,6,"use-after-move"))return 10;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,DROP,0,2);ev(e,2,READ,0,3);if(bad(e,3,1,7,"use-after-drop"))return 11;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,BORROW_MUT,0,2);ev(e,2,READ,0,3);if(bad(e,3,1,8,"read-under-mut-borrow"))return 12;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,BORROW_SHARED,0,2);ev(e,2,WRITE,0,3);if(bad(e,3,1,9,"write-under-shared-borrow"))return 13;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,BORROW_MUT,0,2);ev(e,2,BORROW_SHARED,0,3);if(bad(e,3,1,10,"shared-under-mut"))return 14;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,BORROW_SHARED,0,2);ev(e,2,BORROW_MUT,0,3);if(bad(e,3,1,11,"mut-under-shared"))return 15;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,END_SHARED,0,2);if(bad(e,2,1,12,"end-absent-shared"))return 16;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,END_MUT,0,2);if(bad(e,2,1,13,"end-absent-mut"))return 17;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,BORROW_SHARED,0,2);ev(e,2,MOVE,0,3);if(bad(e,3,1,14,"move-while-borrowed"))return 18;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,BORROW_MUT,0,2);ev(e,2,DROP,0,3);if(bad(e,3,1,15,"drop-while-borrowed"))return 19;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,BORROW_SHARED,0,2);if(bad(e,2,1,16,"borrow-escapes-scope"))return 20;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,0,1);ev(e,1,DECLARE,0,2);if(bad(e,2,1,4,"duplicate-declaration"))return 21;
 memset(e,0,sizeof(e));ev(e,0,99,0,1);if(bad(e,1,1,2,"invalid-event"))return 22;
 memset(e,0,sizeof(e));ev(e,0,DECLARE,1,1);if(bad(e,1,1,3,"invalid-symbol"))return 23;
 memset(e,0,sizeof(e));ev(e,0,READ,0,1);if(bad(e,1,1,5,"read-before-declare"))return 24;
 return 0;
}
int main(void){
 int r=valid();if(r)return r;r=negative();if(r)return r;
 uint32_t w[8]={0},e[4]={0};
 if(swan_fn_swanc_ownership_analyze(e,0,2,w,11)!=-1||w[0]!=1)return 30;
 memset(w,0,sizeof(w));
 if(swan_fn_swanc_ownership_analyze(e,0,0,w,3)!=-1||w[0]!=0)return 31;
 puts("Compiler 2 ownership verifier passed: ownership, shared/exclusive borrowing, moves, drops, use-after-move/drop, conflicts, and malformed events.");
 return 0;
}
