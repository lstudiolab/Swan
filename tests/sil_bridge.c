#include "swan/compiler.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    const char *source =
        "func add(a: int, b: int) -> int:\n"
        "    return a + b\n"
        "func main() -> int:\n"
        "    return 2 + 3 * 4\n"
        "func negative() -> int:\n"
        "    return 1 - 2\n"
        "func wide() -> int:\n"
        "    return 50000 * 50000\n"
        "out\n";
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
    if (swan_sil_try_irgen_program(&program, &module, &error) != SWAN_SIL_LOWERED) {
        fprintf(stderr, "Swan-authored SIL rejected the signed/wide scalar test program\n");
        free(tokens.items);
        free(tokens.text_storage);
        swan_program_destroy(&program);
        return 4;
    }
    if (module.function_count != 4 ||
        module.functions[0].parameter_count != 2 ||
        module.functions[0].blocks[0].instruction_count != 1 ||
        module.functions[0].blocks[0].instructions[0].op != SWAN_IR_ADD ||
        module.functions[1].blocks[0].instruction_count != 1 ||
        module.functions[1].blocks[0].instructions[0].op != SWAN_IR_CONST ||
        module.functions[1].blocks[0].instructions[0].immediate != 14 ||
        module.functions[2].blocks[0].instruction_count != 1 ||
        module.functions[2].blocks[0].instructions[0].op != SWAN_IR_CONST ||
        module.functions[2].blocks[0].instructions[0].immediate != -1 ||
        module.functions[3].blocks[0].instruction_count != 3 ||
        module.functions[3].blocks[0].instructions[2].op != SWAN_IR_MUL) {
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
    puts("Swan-authored SIL -> verified typed SSA IR passed, including negative and wide constants.");
    swan_ir_destroy(&module);
    free(tokens.items);
    free(tokens.text_storage);
    swan_program_destroy(&program);
    return 0;
}
