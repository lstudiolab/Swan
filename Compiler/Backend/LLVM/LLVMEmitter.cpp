#include "Swan/Backend/LLVM/LLVMEmitter.hpp"

#include <cstdint>
#include <map>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace swan::backend::llvm {
namespace {

struct StringGlobal {
    std::string symbol;
    std::string bytes;
};

std::string functionSymbol(const std::string& name) {
    return name == "main" ? "@main" : "@swan.fn." + name;
}

std::string valueName(sil::ValueID value) {
    return "%v" + std::to_string(value);
}

std::string blockName(sil::BlockID block) {
    return "bb" + std::to_string(block);
}

std::string llvmBytes(const std::string& bytes) {
    std::ostringstream output;
    for (const unsigned char byte : bytes) {
        if (byte >= 32 && byte <= 126 && byte != '"' && byte != '\\')
            output << static_cast<char>(byte);
        else {
            static const char hex[] = "0123456789ABCDEF";
            output << '\\' << hex[(byte >> 4) & 0x0f] << hex[byte & 0x0f];
        }
    }
    output << "\\00";
    return output.str();
}

std::string decodeString(std::string text) {
    std::size_t delimiter = 1;
    if (text.size() >= 3 && text.compare(0, 3, "\"\"\"") == 0)
        delimiter = 3;
    else if (text.size() >= 3 && text.compare(0, 3, "\xE2\x80\x9C") == 0)
        delimiter = 3;

    if (text.size() >= delimiter * 2)
        text = text.substr(delimiter, text.size() - delimiter * 2);

    std::string result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            result.push_back(text[i]);
            continue;
        }
        const char escaped = text[++i];
        switch (escaped) {
            case 'n': result.push_back('\n'); break;
            case 'r': result.push_back('\r'); break;
            case 't': result.push_back('\t'); break;
            case '0': result.push_back('\0'); break;
            case '\\': result.push_back('\\'); break;
            case '"': result.push_back('"'); break;
            default:
                result.push_back('\\');
                result.push_back(escaped);
                break;
        }
    }
    return result;
}

void report(EmitResult& result, std::string code, std::string message) {
    result.diagnostics.emplace_back(SourceLocation{}, std::move(code), std::move(message));
}

} // namespace

EmitResult LLVMEmitter::emit(const sil::Module& module) const {
    EmitResult result;
    sil::Verifier verifier;
    if (!verifier.verify(module, result.diagnostics)) return result;

    std::unordered_map<std::string, sil::Type> functionTypes;
    std::map<sil::ValueID, StringGlobal> strings;
    std::size_t stringIndex = 0;
    bool needsPrintf = false;

    for (const auto& function : module.functions) {
        functionTypes[function.name] = function.returnType;
        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                if (instruction.opcode == sil::Opcode::ConstantString) {
                    strings[instruction.result] = {
                        "@swan.str." + std::to_string(stringIndex++),
                        decodeString(instruction.text)
                    };
                }
                if (instruction.opcode == sil::Opcode::Log) needsPrintf = true;
            }
        }
    }

    std::ostringstream output;
    output << "; Swan LLVM IR\n";
    output << '\n';

    for (const auto& entry : strings) {
        const auto length = entry.second.bytes.size() + 1;
        output << entry.second.symbol << " = private unnamed_addr constant ["
               << length << " x i8] c\"" << llvmBytes(entry.second.bytes)
               << "\", align 1\n";
    }
    if (needsPrintf) {
        output << "@swan.fmt.int = private unnamed_addr constant [5 x i8] c\"%ld\\0A\\00\", align 1\n";
        output << "@swan.fmt.str = private unnamed_addr constant [4 x i8] c\"%s\\0A\\00\", align 1\n";
        output << "declare i32 @printf(ptr, ...)\n";
    }
    if (!strings.empty() || needsPrintf) output << '\n';

    for (const auto& function : module.functions) {
        if (function.returnType == sil::Type::F64)
            report(result, "B005", "Floating-point function returns are not supported by the current LLVM ABI.");
        for (const auto& parameter : function.parameters) {
            if (parameter.type == sil::Type::F64)
                report(result, "B006", "Floating-point parameters are not supported by the current LLVM ABI.");
        }
        std::unordered_map<sil::ValueID, sil::Type> valueTypes;
        for (const auto& parameter : function.parameters)
            valueTypes[parameter.value] = parameter.type;
        for (const auto& block : function.blocks) {
            for (const auto& instruction : block.instructions) {
                if (!instruction.result) continue;
                auto type = instruction.type;
                if (instruction.opcode == sil::Opcode::Load && !instruction.operands.empty()) {
                    const auto found = valueTypes.find(instruction.operands.front());
                    if (found != valueTypes.end()) type = found->second;
                }
                valueTypes[instruction.result] = type;
            }
        }

        output << "define " << (function.returnType == sil::Type::Void ? "void" : "i64")
               << ' ' << functionSymbol(function.name) << '(';
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            if (i) output << ", ";
            output << "i64 " << valueName(function.parameters[i].value);
        }
        output << ") {\n";

        for (const auto& block : function.blocks) {
            output << blockName(block.id) << ":\n";
            for (const auto& instruction : block.instructions) {
                const auto resultName = instruction.result
                    ? valueName(instruction.result) : std::string{};
                const auto operand = [&](std::size_t index) {
                    return index < instruction.operands.size()
                        ? valueName(instruction.operands[index]) : std::string("0");
                };

                switch (instruction.opcode) {
                    case sil::Opcode::ConstantInt:
                    case sil::Opcode::ConstantBool:
                        output << "  " << resultName << " = add i64 0, "
                               << instruction.integer << "\n";
                        break;
                    case sil::Opcode::ConstantNone:
                        output << "  " << resultName << " = add i64 0, 0\n";
                        break;
                    case sil::Opcode::ConstantFloat:
                        report(result, "B001", "LLVM backend does not yet lower floating-point SIL values.");
                        break;
                    case sil::Opcode::ConstantString: {
                        const auto found = strings.find(instruction.result);
                        if (found == strings.end()) {
                            report(result, "B002", "Missing string constant storage.");
                            break;
                        }
                        const auto length = found->second.bytes.size() + 1;
                        const auto pointer = "%strptr" + std::to_string(instruction.result);
                        output << "  " << pointer << " = getelementptr inbounds ["
                               << length << " x i8], ptr "
                               << found->second.symbol << ", i64 0, i64 0\n";
                        output << "  " << resultName << " = ptrtoint ptr "
                               << pointer << " to i64\n";
                        break;
                    }
                    case sil::Opcode::Load:
                        output << "  " << resultName << " = add i64 " << operand(0)
                               << ", 0\n";
                        break;
                    case sil::Opcode::Store:
                        break;
                    case sil::Opcode::Add:
                        output << "  " << resultName << " = add i64 " << operand(0)
                               << ", " << operand(1) << "\n";
                        break;
                    case sil::Opcode::Subtract:
                        output << "  " << resultName << " = sub i64 " << operand(0)
                               << ", " << operand(1) << "\n";
                        break;
                    case sil::Opcode::Multiply:
                        output << "  " << resultName << " = mul i64 " << operand(0)
                               << ", " << operand(1) << "\n";
                        break;
                    case sil::Opcode::Divide:
                        output << "  " << resultName << " = sdiv i64 " << operand(0)
                               << ", " << operand(1) << "\n";
                        break;
                    case sil::Opcode::Remainder:
                        output << "  " << resultName << " = srem i64 " << operand(0)
                               << ", " << operand(1) << "\n";
                        break;
                    case sil::Opcode::Negate:
                        output << "  " << resultName << " = sub i64 0, " << operand(0) << "\n";
                        break;
                    case sil::Opcode::LogicalNot:
                        output << "  %not" << instruction.result << " = icmp eq i64 "
                               << operand(0) << ", 0\n";
                        output << "  " << resultName << " = zext i1 %not"
                               << instruction.result << " to i64\n";
                        break;
                    case sil::Opcode::LogicalAnd:
                    case sil::Opcode::LogicalOr: {
                        const auto suffix = std::to_string(instruction.result);
                        output << "  %lhs" << suffix << " = icmp ne i64 " << operand(0) << ", 0\n";
                        output << "  %rhs" << suffix << " = icmp ne i64 " << operand(1) << ", 0\n";
                        output << "  %logic" << suffix << " = "
                               << (instruction.opcode == sil::Opcode::LogicalAnd ? "and" : "or")
                               << " i1 %lhs" << suffix << ", %rhs" << suffix << "\n";
                        output << "  " << resultName << " = zext i1 %logic"
                               << suffix << " to i64\n";
                        break;
                    }
                    case sil::Opcode::Equal:
                    case sil::Opcode::NotEqual:
                    case sil::Opcode::Less:
                    case sil::Opcode::LessEqual:
                    case sil::Opcode::Greater:
                    case sil::Opcode::GreaterEqual: {
                        const char* predicate = "eq";
                        switch (instruction.opcode) {
                            case sil::Opcode::NotEqual: predicate = "ne"; break;
                            case sil::Opcode::Less: predicate = "slt"; break;
                            case sil::Opcode::LessEqual: predicate = "sle"; break;
                            case sil::Opcode::Greater: predicate = "sgt"; break;
                            case sil::Opcode::GreaterEqual: predicate = "sge"; break;
                            default: break;
                        }
                        output << "  %cmp" << instruction.result << " = icmp " << predicate
                               << " i64 " << operand(0) << ", " << operand(1) << "\n";
                        output << "  " << resultName << " = zext i1 %cmp"
                               << instruction.result << " to i64\n";
                        break;
                    }
                    case sil::Opcode::Call: {
                        const auto found = functionTypes.find(instruction.text);
                        if (found == functionTypes.end()) {
                            report(result, "B003", "No LLVM function definition for call to '" +
                                   instruction.text + "'.");
                            break;
                        }
                        const bool returnsVoid = found->second == sil::Type::Void;
                        if (returnsVoid)
                            output << "  ";
                        else
                            output << "  " << resultName << " = ";
                        output << "call " << (returnsVoid ? "void" : "i64") << ' '
                               << functionSymbol(instruction.text) << '(';
                        for (std::size_t i = 0; i < instruction.operands.size(); ++i) {
                            if (i) output << ", ";
                            output << "i64 " << operand(i);
                        }
                        output << ")\n";
                        if (returnsVoid)
                            output << "  " << resultName << " = add i64 0, 0\n";
                        break;
                    }
                    case sil::Opcode::Log: {
                        if (!needsPrintf || instruction.operands.empty()) {
                            report(result, "B004", "Malformed log instruction.");
                            break;
                        }
                        const auto found = valueTypes.find(instruction.operands.front());
                        const bool isString = found != valueTypes.end() &&
                                              found->second == sil::Type::String;
                        if (isString) {
                            output << "  %logstr" << instruction.result
                                   << " = inttoptr i64 " << operand(0) << " to ptr\n";
                        }
                        output << "  %log" << instruction.result << " = call i32 (ptr, ...) @printf(ptr "
                               << "getelementptr inbounds ("
                               << (isString ? "[4 x i8], ptr @swan.fmt.str"
                                            : "[5 x i8], ptr @swan.fmt.int")
                               << ", i64 0, i64 0), ";
                        if (isString)
                            output << "ptr %logstr" << instruction.result;
                        else
                            output << "i64 " << operand(0);
                        output << ")\n";
                        output << "  " << resultName << " = zext i32 %log"
                               << instruction.result << " to i64\n";
                        break;
                    }
                    case sil::Opcode::Jump:
                        output << "  br label %" << blockName(instruction.trueTarget) << "\n";
                        break;
                    case sil::Opcode::Branch:
                        output << "  %cond" << block.id << " = icmp ne i64 "
                               << operand(0) << ", 0\n";
                        output << "  br i1 %cond" << block.id << ", label %"
                               << blockName(instruction.trueTarget) << ", label %"
                               << blockName(instruction.falseTarget) << "\n";
                        break;
                    case sil::Opcode::Return:
                        if (function.returnType == sil::Type::Void)
                            output << "  ret void\n";
                        else
                            output << "  ret i64 " << operand(0) << "\n";
                        break;
                }
            }
        }
        output << "}\n\n";
    }

    if (!result.diagnostics.empty()) result.output.clear();
    else result.output = output.str();
    return result;
}

} // namespace swan::backend::llvm
