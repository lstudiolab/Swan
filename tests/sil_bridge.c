#include "swan/compiler.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *source =
        "fn add(a: int, b: int) -> int:\n"
        "    return a + b\n"
        "fn main() -> int:\n"
        "    return 2 + 3 * 4\n";
    SwanError error = {0};
    TokenStream tokens;
    if (!swan_lex(source, &tokens, &error)) {
        fprintf(stderr, "SIL bridge lexer: %s\n", error.message);
        return 1;
    }
    Program program = {0};
    if (!swan_parse(&tokens, &program, &error)) {
        fprintf(stderr, "SIL bridge parser: %s\n", error.message);
        free(tokens.items);
        free(tokens.text_storage);
        swan_program_destroy(&program);
        return 2;
    }
    program.source_function_count = program.count;
    if (!swan_semantic_check(&program, &error)) {
        fprintf(stderr, "SIL bridge sema: %s\n", error.message);
        free(tokens.items);
        free(tokens.text_storage);
        swan_program_destroy(&program);
        return 3;
    }

    SwanIRModule module = {0};
    if (!swan_sil_try_irgen_program(&program, &module)) {
        fprintf(stderr, "Swan-authored SIL rejected the scalar test program\n");
        free(tokens.items);
        free(tokens.text_storage);
        swan_program_destroy(&program);
        return 4;
    }
    if (module.function_count != 2 ||
        module.functions[0].parameter_count != 2 ||
        module.functions[0].blocks[0].instruction_count != 1 ||
        module.functions[0].blocks[0].instructions[0].op != SWAN_IR_ADD ||
        module.functions[1].blocks[0].instruction_count != 1 ||
        module.functions[1].blocks[0].instructions[0].op != SWAN_IR_CONST ||
        module.functions[1].blocks[0].instructions[0].immediate != 14) {
        fprintf(stderr, "SIL-to-IR lowering produced unexpected instructions\n");
        swan_ir_destroy(&module);
        free(tokens.items);
        free(tokens.text_storage);
        swan_program_destroy(&program);
        return 5;
    }
    char detail[256] = {0};
    if (!swan_ir_verify(&module, detail, sizeof(detail))) {
        fprintf(stderr, "SIL-to-IR verifier: %s\n", detail);
        swan_ir_destroy(&module);
        free(tokens.items);
        free(tokens.text_storage);
        swan_program_destroy(&program);
        return 6;
    }

    puts("Swan-authored SIL -> verified typed SSA IR passed.");
    swan_ir_destroy(&module);
    free(tokens.items);
    free(tokens.text_storage);
    swan_program_destroy(&program);
    return 0;
}
