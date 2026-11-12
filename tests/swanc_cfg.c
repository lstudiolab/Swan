#include <stdint.h>
#include <stdio.h>
#include <string.h>
extern int64_t swan_fn_swanc_cfg_verify(uint32_t *, int64_t, uint32_t *, int64_t);

enum { RETURN=1, BRANCH=2, COND_BRANCH=3, UNREACHABLE=4 };
static void block(uint32_t *g, uint32_t id, uint32_t term, uint32_t a,
                  uint32_t b, uint32_t preds, uint32_t flags) {
    uint32_t p = id * 5;
    g[p] = term; g[p+1] = a; g[p+2] = b; g[p+3] = preds; g[p+4] = flags;
}
static int reject(uint32_t *g, int n, uint32_t code, const char *name) {
    uint32_t w[64] = {0};
    int64_t r = swan_fn_swanc_cfg_verify(g, n, w, 64);
    if (r == 0 || w[0] != code) {
        fprintf(stderr, "%s: status=%lld diagnostic=%u expected=%u\n",
                name, (long long)r, w[0], code);
        return 1;
    }
    return 0;
}
static int valid_graph(void) {
    uint32_t g[25] = {0}, w[64] = {0};
    block(g,0,COND_BRANCH,2,3,0,0);
    block(g,1,BRANCH,4,0,1,0);
    block(g,2,BRANCH,4,0,1,0);
    block(g,3,RETURN,0,0,2,0);
    block(g,4,RETURN,0,0,0,0); /* unreachable, but structurally valid */
    int64_t r = swan_fn_swanc_cfg_verify(g,5,w,19);
    if (r != 0 || w[0] != 0 || w[3] != 4) {
        fprintf(stderr,"valid CFG rejected: status=%lld code=%u reachable=%u\n",
                (long long)r,w[0],w[3]);
        return 1;
    }
    return 0;
}
int main(void) {
    fprintf(stderr, "cfg-test-start\n");
    fprintf(stderr, "valid-start\n");
    if (valid_graph()) return 1;
    fprintf(stderr, "valid-done\n");
    uint32_t g[20] = {0};
    block(g,0,RETURN,0,0,0,0);
    block(g,1,RETURN,0,0,0,0);
    block(g,2,RETURN,0,0,0,0);
    block(g,3,RETURN,0,0,0,0);
    block(g,0,99,0,0,0,0);
    if (reject(g,1,4,"invalid terminator")) return 2;
    block(g,0,RETURN,2,0,0,0);
    if (reject(g,1,5,"return with successor")) return 3;
    block(g,0,BRANCH,2,0,0,0);
    if (reject(g,1,6,"out-of-range branch")) return 4;
    block(g,0,COND_BRANCH,1,1,0,0);
    if (reject(g,1,9,"predecessor mismatch")) return 5;
    block(g,0,RETURN,0,0,0,1);
    if (reject(g,1,3,"reserved flags")) return 6;
    block(g,0,BRANCH,1,0,1,0);
    if (reject(g,1,10,"entry predecessor")) return 7;
    block(g,0,RETURN,0,0,0,0);
    uint32_t w[8] = {0};
    if (swan_fn_swanc_cfg_verify(g,1,w,6) != -2 || w[0] != 2) return 8;
    if (swan_fn_swanc_cfg_verify(g,0,w,8) != -1 || w[0] != 1) return 9;
    puts("Compiler 2 CFG verifier passed: terminators, edges, predecessor counts, entry invariants, bounded reachability, and unreachable-block accounting.");
    return 0;
}
