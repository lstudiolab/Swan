#include "brew/lexer.h"
#include "brew/sema.h"
#include "brew/irgen.h"
#include "brew/optimization.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    const char *source=
        "func explode() -> bool:\n"
        "    return 1 / 0 == 0\n"
        "func double(x: int) -> int:\n"
        "    return x * 2\n"
        "func noop() -> void:\n"
        "    pass\n"
        "func minimum() -> int:\n"
        "    return -9223372036854775808\n"
        "func main() -> int:\n"
        "    noop()\n"
        "    var i = 0\n"
        "    var total = 0\n"
        "    while i < 10:\n"
        "        i = i + 1\n"
        "        if i == 3:\n"
        "            continue\n"
        "        if i == 8:\n"
        "            break\n"
        "        total = total + double(i)\n"
        "    if false and explode():\n"
        "        return 1\n"
        "    if true or explode():\n"
        "        return total\n"
        "    return 0\n"
        ;
    BrewError error={0}; TokenStream tokens={0}; Program program={0};
    assert(brew_lex(source,&tokens,&error));
    assert(brew_parse(&tokens,&program,&error));
    int64_t minimum=0;
    bool found_minimum=false;
    for(size_t i=0;i<program.count;i++){
        Stmt *function=program.functions[i];
        if(function&&!strcmp(function->name,"minimum")&&function->body_count){
            found_minimum=brew_irgen_integer_literal(function->body[0]->value,&minimum);
            break;
        }
    }
    assert(found_minimum&&minimum==INT64_MIN);
    assert(brew_semantic_check(&program,&error));
    BrewIRModule module={0};
    assert(brew_irgen_program(&program,&module,&error));
    bool emitted_minimum=false;
    for(size_t f=0;f<module.function_count;f++)
        for(size_t b=0;b<module.functions[f].block_count;b++)
            for(size_t i=0;i<module.functions[f].blocks[b].instruction_count;i++){
                BrewIRInst *instruction=&module.functions[f].blocks[b].instructions[i];
                if(instruction->op==BREW_IR_CONST&&instruction->immediate==INT64_MIN)
                    emitted_minimum=true;
            }
    assert(emitted_minimum);
    char detail[256]={0};
    assert(brew_ir_optimize(&module,detail,sizeof(detail)));
    assert(brew_ir_verify(&module,detail,sizeof(detail)));
    brew_ir_destroy(&module);
    /* The diagnostic sink is optional, including for void-call statements. */
    assert(brew_irgen_program(&program,&module,NULL));
    brew_ir_destroy(&module);
    brew_program_destroy(&program);free(tokens.items);free(tokens.text_storage);
    puts("PASS: .brew -> typed IR -> verified optimization, loops, branches and calls");
    return 0;
}
