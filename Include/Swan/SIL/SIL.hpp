#pragma once

#include "Swan/AST/AST.hpp"
#include "Swan/Sema/Diagnostic.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace swan::sil {

using ValueID = std::uint32_t;
using BlockID = std::uint32_t;

enum class Type { Void, I64, F64, Bool, String, Unknown };
enum class Opcode {
    ConstantInt, ConstantFloat, ConstantBool, ConstantString, ConstantNone,
    Load, Store,
    Add, Subtract, Multiply, Divide, Remainder,
    Negate, LogicalNot,
    Equal, NotEqual, Less, LessEqual, Greater, GreaterEqual,
    Call, Log,
    Jump, Branch, Return
};

struct Instruction {
    Opcode opcode = Opcode::ConstantNone;
    ValueID result = 0;
    Type type = Type::Unknown;
    std::vector<ValueID> operands;
    std::string text;
    std::int64_t integer = 0;
    double floating = 0.0;
    BlockID trueTarget = 0;
    BlockID falseTarget = 0;
};

struct BasicBlock {
    BlockID id = 0;
    std::string name;
    std::vector<Instruction> instructions;
};

struct Parameter {
    std::string name;
    Type type = Type::Unknown;
    ValueID value = 0;
};

struct Function {
    std::string name;
    std::vector<Parameter> parameters;
    Type returnType = Type::Unknown;
    std::vector<BasicBlock> blocks;
};

struct Module {
    std::vector<Function> functions;
};

struct Result {
    Module module;
    std::vector<SemaDiagnostic> diagnostics;
    bool ok() const noexcept { return diagnostics.empty(); }
};

class Lowerer {
public:
    Result lower(const swan::Module& module);
};

class Verifier {
public:
    bool verify(const Module& module, std::vector<SemaDiagnostic>& diagnostics) const;
};

std::string toString(Type type);
std::string toString(Opcode opcode);
std::string print(const Module& module);

} // namespace swan::sil
