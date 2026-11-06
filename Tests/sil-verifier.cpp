#include "Swan/SIL/SIL.hpp"

#include <utility>
#include <vector>

int main() {
    swan::sil::Module module;
    swan::sil::Function function;
    function.name = "invalid_dominance";
    function.returnType = swan::sil::Type::I64;

    swan::sil::Instruction condition;
    condition.opcode = swan::sil::Opcode::ConstantBool;
    condition.result = 1;
    condition.type = swan::sil::Type::Bool;
    condition.integer = 1;

    swan::sil::Instruction branch;
    branch.opcode = swan::sil::Opcode::Branch;
    branch.operands = {1};
    branch.trueTarget = 1;
    branch.falseTarget = 2;

    swan::sil::Instruction branchValue;
    branchValue.opcode = swan::sil::Opcode::ConstantInt;
    branchValue.result = 2;
    branchValue.type = swan::sil::Type::I64;
    branchValue.integer = 7;

    swan::sil::Instruction jump;
    jump.opcode = swan::sil::Opcode::Jump;
    jump.trueTarget = 2;

    swan::sil::Instruction ret;
    ret.opcode = swan::sil::Opcode::Return;
    ret.operands = {2};

    function.blocks = {
        {0, "entry", {condition, branch}},
        {1, "then", {branchValue, jump}},
        {2, "merge", {ret}}
    };
    module.functions.push_back(std::move(function));

    std::vector<swan::SemaDiagnostic> diagnostics;
    swan::sil::Verifier verifier;
    if (verifier.verify(module, diagnostics)) return 1;

    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.code == "V011") return 0;
    }
    return 2;
}
