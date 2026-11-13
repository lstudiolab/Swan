#ifndef SWAN_EMITTER_H
#define SWAN_EMITTER_H
#include "swan/codegen.h"

/* Assembly serialization is deliberately separate from target selection. */
static inline bool swan_emit_assembly(FILE *output, const Text *assembly) {
    if (!output || !assembly || (assembly->len && !assembly->data)) return false;
    if (assembly->len && fwrite(assembly->data,1,assembly->len,output)!=assembly->len) return false;
    return fflush(output)==0;
}
#endif
