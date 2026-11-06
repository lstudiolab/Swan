#include "Swan/SIL/SIL.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <unordered_map>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace swan::sil {
namespace {

Type typeFromName(const std::string& name) {
    if (name == "int") return Type::I64;
    if (name == "float") return Type::F64;
    if (name == "bool") return Type::Bool;
    if (name == "string") return Type::String;
    if (name == "none" || name == "void") return Type::Void;
    return Type::Unknown;
}

bool terminates(const BasicBlock& block) {
    if (block.instructions.empty()) return false;
    const auto op = block.instructions.back().opcode;
    return op == Opcode::Jump || op == Opcode::Branch || op == Opcode::Return;
}

class FunctionLowering {
public:
    FunctionLowering(Function& function, std::vector<SemaDiagnostic>& diagnostics)
        : function_(function), diagnostics_(diagnostics) {
        createBlock("entry");
    }

    void lower(const FunctionDecl& declaration) {
        for (const auto& parameter : declaration.parameters) {
            const ValueID id = nextValue_++;
            const Type type = parameter.typeName
                ? typeFromName(*parameter.typeName) : Type::Unknown;
            function_.parameters.push_back({parameter.name, type, id});
            variables_[parameter.name] = id;
        }
        lowerStatements(declaration.body);
        if (!terminates(current())) {
            Instruction result;
            result.opcode = Opcode::ConstantNone;
            result.result = nextValue_++;
            result.type = Type::Void;
            emit(std::move(result));
            Instruction ret;
            ret.opcode = Opcode::Return;
            ret.operands.push_back(nextValue_ - 1);
            emit(std::move(ret));
        }
    }

private:
    Function& function_;
    std::vector<SemaDiagnostic>& diagnostics_;
    BlockID currentBlock_ = 0;
    BlockID nextBlock_ = 0;
    ValueID nextValue_ = 1;
    std::unordered_map<std::string, ValueID> variables_;
    std::vector<std::pair<BlockID, BlockID>> loopTargets_;

    BasicBlock& current() { return function_.blocks[currentBlock_]; }

    BlockID createBlock(std::string name) {
        const BlockID id = static_cast<BlockID>(function_.blocks.size());
        function_.blocks.push_back({id, std::move(name), {}});
        currentBlock_ = id;
        return id;
    }

    void select(BlockID id) { currentBlock_ = id; }
    void emit(Instruction instruction) { current().instructions.push_back(std::move(instruction)); }

    void diagnose(SourceLocation location, std::string message) {
        diagnostics_.emplace_back(location, "L001", std::move(message));
    }

    ValueID constantNone() {
        Instruction instruction;
        instruction.opcode = Opcode::ConstantNone;
        instruction.result = nextValue_++;
        instruction.type = Type::Void;
        const auto id = instruction.result;
        emit(std::move(instruction));
        return id;
    }

    ValueID lowerExpression(const Expr& expression) {
        if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
            Instruction instruction;
            instruction.result = nextValue_++;
            instruction.text = literal->token.lexeme;
            switch (literal->token.kind) {
                case TokenKind::Integer:
                    instruction.opcode = Opcode::ConstantInt;
                    instruction.type = Type::I64;
                    try { instruction.integer = std::stoll(instruction.text); }
                    catch (...) { diagnose(expression.location, "Integer literal is outside the supported 64-bit range."); }
                    break;
                case TokenKind::Float:
                    instruction.opcode = Opcode::ConstantFloat;
                    instruction.type = Type::F64;
                    try { instruction.floating = std::stod(instruction.text); }
                    catch (...) { diagnose(expression.location, "Invalid floating-point literal."); }
                    break;
                case TokenKind::String:
                    instruction.opcode = Opcode::ConstantString;
                    instruction.type = Type::String;
                    break;
                case TokenKind::True:
                case TokenKind::False:
                    instruction.opcode = Opcode::ConstantBool;
                    instruction.type = Type::Bool;
                    instruction.integer = literal->token.kind == TokenKind::True ? 1 : 0;
                    break;
                default:
                    instruction.opcode = Opcode::ConstantNone;
                    instruction.type = Type::Void;
                    break;
            }
            const auto id = instruction.result;
            emit(std::move(instruction));
            return id;
        }

        if (const auto* name = dynamic_cast<const NameExpr*>(&expression)) {
            Instruction instruction;
            instruction.opcode = Opcode::Load;
            instruction.result = nextValue_++;
            instruction.type = Type::Unknown;
            instruction.text = name->name;
            const auto found = variables_.find(name->name);
            if (found == variables_.end()) {
                diagnose(expression.location, "Cannot lower unresolved name '" + name->name + "'.");
                return constantNone();
            }
            instruction.operands.push_back(found->second);
            const auto id = instruction.result;
            emit(std::move(instruction));
            return id;
        }

        if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
            if (!unary->operand) return constantNone();
            const auto value = lowerExpression(*unary->operand);
            Instruction instruction;
            instruction.result = nextValue_++;
            instruction.operands.push_back(value);
            instruction.type = Type::Unknown;
            if (unary->op.kind == TokenKind::Minus) instruction.opcode = Opcode::Negate;
            else if (unary->op.kind == TokenKind::Not) instruction.opcode = Opcode::LogicalNot;
            else if (unary->op.kind == TokenKind::Log) {
                instruction.opcode = Opcode::Log;
                instruction.type = Type::Void;
            } else {
                diagnose(expression.location, "The '" + unary->op.lexeme +
                         "' operation has no finalized SIL lowering yet.");
                return constantNone();
            }
            const auto id = instruction.result;
            emit(std::move(instruction));
            return id;
        }

        if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
            if (!binary->left || !binary->right) return constantNone();
            if (binary->op.kind == TokenKind::Assign) {
                const auto* target = dynamic_cast<const NameExpr*>(binary->left.get());
                if (!target) {
                    diagnose(expression.location, "Only named variables can be assigned in this SIL version.");
                    return lowerExpression(*binary->right);
                }
                const auto value = lowerExpression(*binary->right);
                Instruction store;
                store.opcode = Opcode::Store;
                store.operands.push_back(value);
                store.text = target->name;
                emit(std::move(store));
                variables_[target->name] = value;
                return value;
            }

            const auto left = lowerExpression(*binary->left);
            const auto right = lowerExpression(*binary->right);
            Instruction instruction;
            instruction.result = nextValue_++;
            instruction.operands = {left, right};
            instruction.type = Type::Unknown;
            switch (binary->op.kind) {
                case TokenKind::Plus: instruction.opcode = Opcode::Add; break;
                case TokenKind::Minus: instruction.opcode = Opcode::Subtract; break;
                case TokenKind::Star: instruction.opcode = Opcode::Multiply; break;
                case TokenKind::Slash: instruction.opcode = Opcode::Divide; break;
                case TokenKind::Percent: instruction.opcode = Opcode::Remainder; break;
                case TokenKind::Equal: instruction.opcode = Opcode::Equal; instruction.type = Type::Bool; break;
                case TokenKind::NotEqual: instruction.opcode = Opcode::NotEqual; instruction.type = Type::Bool; break;
                case TokenKind::Less: instruction.opcode = Opcode::Less; instruction.type = Type::Bool; break;
                case TokenKind::LessEqual: instruction.opcode = Opcode::LessEqual; instruction.type = Type::Bool; break;
                case TokenKind::Greater: instruction.opcode = Opcode::Greater; instruction.type = Type::Bool; break;
                case TokenKind::GreaterEqual: instruction.opcode = Opcode::GreaterEqual; instruction.type = Type::Bool; break;
                default:
                    diagnose(expression.location, "This binary operator is not supported by SIL lowering.");
                    return constantNone();
            }
            const auto id = instruction.result;
            emit(std::move(instruction));
            return id;
        }

        if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
            const auto* callee = dynamic_cast<const NameExpr*>(call->callee.get());
            if (!callee) {
                diagnose(expression.location, "Only direct function calls can be lowered currently.");
                return constantNone();
            }
            Instruction instruction;
            instruction.opcode = Opcode::Call;
            instruction.result = nextValue_++;
            instruction.type = Type::Unknown;
            instruction.text = callee->name;
            for (const auto& argument : call->arguments)
                if (argument) instruction.operands.push_back(lowerExpression(*argument));
            const auto id = instruction.result;
            emit(std::move(instruction));
            return id;
        }

        diagnose(expression.location, "This expression form is not supported by SIL lowering.");
        return constantNone();
    }

    void lowerStatements(const std::vector<StmtPtr>& statements) {
        for (const auto& statement : statements) {
            if (!statement || terminates(current())) break;
            lowerStatement(*statement);
        }
    }

    void lowerStatement(const Stmt& statement) {
        if (const auto* variable = dynamic_cast<const VarDecl*>(&statement)) {
            const auto value = variable->initializer
                ? lowerExpression(*variable->initializer) : constantNone();
            variables_[variable->name] = value;
            Instruction store;
            store.opcode = Opcode::Store;
            store.operands.push_back(value);
            store.text = variable->name;
            emit(std::move(store));
            return;
        }
        if (const auto* expression = dynamic_cast<const ExprStmt*>(&statement)) {
            if (expression->expression) lowerExpression(*expression->expression);
            return;
        }
        if (const auto* result = dynamic_cast<const ReturnStmt*>(&statement)) {
            const auto value = result->value ? lowerExpression(*result->value) : constantNone();
            Instruction ret;
            ret.opcode = Opcode::Return;
            ret.operands.push_back(value);
            emit(std::move(ret));
            return;
        }
        if (const auto* conditional = dynamic_cast<const IfStmt*>(&statement)) {
            const auto incomingVariables = variables_;
            const auto condition = conditional->condition
                ? lowerExpression(*conditional->condition) : constantNone();
            if (variables_ != incomingVariables)
                diagnose(statement.location, "Assignments in branch conditions require explicit SSA merge support.");
            const auto thenBlock = static_cast<BlockID>(function_.blocks.size());
            function_.blocks.push_back({thenBlock, "if.then", {}});
            const auto elseBlock = static_cast<BlockID>(function_.blocks.size());
            function_.blocks.push_back({elseBlock, "if.else", {}});
            const auto mergeBlock = static_cast<BlockID>(function_.blocks.size());
            function_.blocks.push_back({mergeBlock, "if.end", {}});
            Instruction branch;
            branch.opcode = Opcode::Branch;
            branch.operands.push_back(condition);
            branch.trueTarget = thenBlock;
            branch.falseTarget = elseBlock;
            emit(std::move(branch));

            select(thenBlock);
            variables_ = incomingVariables;
            lowerStatements(conditional->body);
            const auto thenVariables = variables_;
            if (!terminates(current())) {
                Instruction jump; jump.opcode = Opcode::Jump; jump.trueTarget = mergeBlock;
                emit(std::move(jump));
            }
            select(elseBlock);
            variables_ = incomingVariables;
            lowerStatements(conditional->elseBody);
            const auto elseVariables = variables_;
            if (!terminates(current())) {
                Instruction jump; jump.opcode = Opcode::Jump; jump.trueTarget = mergeBlock;
                emit(std::move(jump));
            }
            for (const auto& entry : incomingVariables) {
                const auto thenValue = thenVariables.find(entry.first);
                const auto elseValue = elseVariables.find(entry.first);
                if ((thenValue != thenVariables.end() && thenValue->second != entry.second) ||
                    (elseValue != elseVariables.end() && elseValue->second != entry.second))
                    diagnose(statement.location, "Assignments across branches require phi-node support and are not lowered yet.");
            }
            variables_ = incomingVariables;
            select(mergeBlock);
            return;
        }
        if (const auto* loop = dynamic_cast<const WhileStmt*>(&statement)) {
            const auto incomingVariables = variables_;
            const auto header = static_cast<BlockID>(function_.blocks.size());
            function_.blocks.push_back({header, "while.cond", {}});
            const auto body = static_cast<BlockID>(function_.blocks.size());
            function_.blocks.push_back({body, "while.body", {}});
            const auto exit = static_cast<BlockID>(function_.blocks.size());
            function_.blocks.push_back({exit, "while.end", {}});
            Instruction enter; enter.opcode = Opcode::Jump; enter.trueTarget = header;
            emit(std::move(enter));
            select(header);
            const auto condition = loop->condition
                ? lowerExpression(*loop->condition) : constantNone();
            Instruction branch; branch.opcode = Opcode::Branch;
            branch.operands.push_back(condition);
            branch.trueTarget = body; branch.falseTarget = exit;
            emit(std::move(branch));
            select(body);
            loopTargets_.push_back({exit, header});
            lowerStatements(loop->body);
            loopTargets_.pop_back();
            if (!terminates(current())) {
                Instruction back; back.opcode = Opcode::Jump; back.trueTarget = header;
                emit(std::move(back));
            }
            for (const auto& entry : incomingVariables) {
                const auto changed = variables_.find(entry.first);
                if (changed != variables_.end() && changed->second != entry.second)
                    diagnose(statement.location, "Assignments in loops require loop-carried phi nodes and are not lowered yet.");
            }
            variables_ = incomingVariables;
            select(exit);
            return;
        }
        if (const auto* control = dynamic_cast<const ControlStmt*>(&statement)) {
            if (control->kind == TokenKind::Pass) return;
            if (control->kind == TokenKind::Break ||
                control->kind == TokenKind::Continue) {
                if (loopTargets_.empty()) {
                    diagnose(statement.location, "Loop control has no active loop target.");
                    return;
                }
                Instruction jump;
                jump.opcode = Opcode::Jump;
                jump.trueTarget = control->kind == TokenKind::Break
                    ? loopTargets_.back().first : loopTargets_.back().second;
                emit(std::move(jump));
                return;
            }
            diagnose(statement.location, "This control statement is not supported by SIL lowering.");
            return;
        }
        if (dynamic_cast<const FunctionDecl*>(&statement) ||
            dynamic_cast<const StructDecl*>(&statement) ||
            dynamic_cast<const ImportStmt*>(&statement)) {
            diagnose(statement.location, "Nested declarations, structures, and imports are not lowered inside a function.");
            return;
        }
        diagnose(statement.location, "This statement form is not supported by SIL lowering.");
    }
};

} // namespace

Result Lowerer::lower(const swan::Module& source) {
    Result result;
    result.module.functions.reserve(source.statements.size());
    for (const auto& statement : source.statements) {
        const auto* declaration = dynamic_cast<const FunctionDecl*>(statement.get());
        if (!declaration) {
            if (statement)
                result.diagnostics.emplace_back(statement->location, "L002",
                    "SIL currently requires executable code to be inside a function.");
            continue;
        }
        Function function;
        function.name = declaration->name;
        function.returnType = declaration->returnType
            ? typeFromName(*declaration->returnType) : Type::Unknown;
        FunctionLowering lowering(function, result.diagnostics);
        lowering.lower(*declaration);
        result.module.functions.push_back(std::move(function));
    }
    Verifier verifier;
    verifier.verify(result.module, result.diagnostics);
    return result;
}

bool Verifier::verify(const Module& module,
                      std::vector<SemaDiagnostic>& diagnostics) const {
    bool valid = true;
    for (const auto& function : module.functions) {
        std::unordered_set<ValueID> values;
        for (const auto& parameter : function.parameters) {
            if (parameter.value == 0 || !values.insert(parameter.value).second) {
                diagnostics.emplace_back(SourceLocation{}, "V001",
                    "Invalid or duplicate parameter value in function '" + function.name + "'.");
                valid = false;
            }
        }
        if (function.blocks.empty()) {
            diagnostics.emplace_back(SourceLocation{}, "V002",
                "Function '" + function.name + "' has no basic blocks.");
            valid = false;
            continue;
        }
        for (const auto& block : function.blocks) {
            if (block.instructions.empty() || !terminates(block)) {
                diagnostics.emplace_back(SourceLocation{}, "V003",
                    "Basic block '" + block.name + "' in function '" +
                    function.name + "' has no terminator.");
                valid = false;
            }
            for (const auto& instruction : block.instructions) {
                if (instruction.result && !values.insert(instruction.result).second) {
                    diagnostics.emplace_back(SourceLocation{}, "V004",
                        "Duplicate SSA value identifier in function '" + function.name + "'.");
                    valid = false;
                }
                for (const auto operand : instruction.operands) {
                    if (operand == 0 || values.find(operand) == values.end()) {
                        diagnostics.emplace_back(SourceLocation{}, "V005",
                            "Instruction references an undefined or forward SSA value in function '" +
                            function.name + "'.");
                        valid = false;
                    }
                }
                if (instruction.opcode == Opcode::Jump &&
                    instruction.trueTarget >= function.blocks.size()) {
                    diagnostics.emplace_back(SourceLocation{}, "V006", "Jump target is out of range.");
                    valid = false;
                }
                if (instruction.opcode == Opcode::Branch &&
                    (instruction.trueTarget >= function.blocks.size() ||
                     instruction.falseTarget >= function.blocks.size())) {
                    diagnostics.emplace_back(SourceLocation{}, "V007", "Branch target is out of range.");
                    valid = false;
                }
            }
        }
    }
    return valid;
}


void Optimizer::optimize(Module& module) const {
    struct Constant {
        std::int64_t value;
        bool boolean;
    };

    for (auto& function : module.functions) {
        std::unordered_map<ValueID, Constant> constants;

        // Fold in block order while retaining constants across basic blocks.
        for (auto& block : function.blocks) {
            for (auto& instruction : block.instructions) {
                if (instruction.opcode == Opcode::ConstantInt ||
                    instruction.opcode == Opcode::ConstantBool) {
                    constants[instruction.result] = {
                        instruction.integer, instruction.opcode == Opcode::ConstantBool
                    };
                    continue;
                }

                if (instruction.operands.size() == 1) {
                    const auto found = constants.find(instruction.operands[0]);
                    if (found != constants.end()) {
                        bool folded = false;
                        bool boolean = false;
                        std::int64_t value = 0;
                        if (instruction.opcode == Opcode::Negate &&
                            found->second.value != std::numeric_limits<std::int64_t>::min()) {
                            value = -found->second.value;
                            folded = true;
                        } else if (instruction.opcode == Opcode::LogicalNot) {
                            value = found->second.value == 0 ? 1 : 0;
                            boolean = true;
                            folded = true;
                        }
                        if (folded) {
                            instruction.opcode = boolean ? Opcode::ConstantBool : Opcode::ConstantInt;
                            instruction.type = boolean ? Type::Bool : Type::I64;
                            instruction.integer = value;
                            instruction.operands.clear();
                            instruction.text.clear();
                            constants[instruction.result] = {value, boolean};
                            continue;
                        }
                    }
                }

                if (instruction.operands.size() != 2) continue;
                const auto left = constants.find(instruction.operands[0]);
                const auto right = constants.find(instruction.operands[1]);
                if (left == constants.end() || right == constants.end()) continue;

                const auto a = left->second.value;
                const auto b = right->second.value;
                std::int64_t value = 0;
                bool folded = true;
                bool boolean = false;

                switch (instruction.opcode) {
                    case Opcode::Add:
                        if ((b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) ||
                            (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b))
                            folded = false;
                        else value = a + b;
                        break;
                    case Opcode::Subtract:
                        if ((b < 0 && a > std::numeric_limits<std::int64_t>::max() + b) ||
                            (b > 0 && a < std::numeric_limits<std::int64_t>::min() + b))
                            folded = false;
                        else value = a - b;
                        break;
                    case Opcode::Multiply:
                        if (a == 0 || b == 0) value = 0;
                        else if ((a == -1 && b == std::numeric_limits<std::int64_t>::min()) ||
                                 (b == -1 && a == std::numeric_limits<std::int64_t>::min()))
                            folded = false;
                        else if (a > 0 ? (b > 0 ? a > std::numeric_limits<std::int64_t>::max() / b
                                                 : b < std::numeric_limits<std::int64_t>::min() / a)
                                       : (b > 0 ? a < std::numeric_limits<std::int64_t>::min() / b
                                                : a < std::numeric_limits<std::int64_t>::max() / b))
                            folded = false;
                        else value = a * b;
                        break;
                    case Opcode::Divide:
                        if (b == 0 || (a == std::numeric_limits<std::int64_t>::min() && b == -1))
                            folded = false;
                        else value = a / b;
                        break;
                    case Opcode::Remainder:
                        if (b == 0 || (a == std::numeric_limits<std::int64_t>::min() && b == -1))
                            folded = false;
                        else value = a % b;
                        break;
                    case Opcode::Equal: value = a == b; boolean = true; break;
                    case Opcode::NotEqual: value = a != b; boolean = true; break;
                    case Opcode::Less: value = a < b; boolean = true; break;
                    case Opcode::LessEqual: value = a <= b; boolean = true; break;
                    case Opcode::Greater: value = a > b; boolean = true; break;
                    case Opcode::GreaterEqual: value = a >= b; boolean = true; break;
                    default: folded = false; break;
                }

                if (folded) {
                    instruction.opcode = boolean ? Opcode::ConstantBool : Opcode::ConstantInt;
                    instruction.type = boolean ? Type::Bool : Type::I64;
                    instruction.integer = value;
                    instruction.operands.clear();
                    instruction.text.clear();
                    constants[instruction.result] = {value, boolean};
                }
            }
        }

        // Count uses across the whole function so values shared by blocks survive.
        std::unordered_map<ValueID, std::size_t> uses;
        for (const auto& block : function.blocks)
            for (const auto& instruction : block.instructions)
                for (const auto operand : instruction.operands) ++uses[operand];

        const auto sideEffecting = [](Opcode opcode) {
            return opcode == Opcode::Store || opcode == Opcode::Call ||
                   opcode == Opcode::Log || opcode == Opcode::Jump ||
                   opcode == Opcode::Branch || opcode == Opcode::Return;
        };
        for (auto& block : function.blocks) {
            block.instructions.erase(
                std::remove_if(block.instructions.begin(), block.instructions.end(),
                    [&](const Instruction& instruction) {
                        return instruction.result != 0 &&
                               uses[instruction.result] == 0 &&
                               !sideEffecting(instruction.opcode);
                    }),
                block.instructions.end());
        }
    }
}

std::string toString(Type type) {
    switch (type) {
        case Type::Void: return "void";
        case Type::I64: return "i64";
        case Type::F64: return "f64";
        case Type::Bool: return "bool";
        case Type::String: return "string";
        default: return "unknown";
    }
}

std::string toString(Opcode opcode) {
    switch (opcode) {
        case Opcode::ConstantInt: return "const.int";
        case Opcode::ConstantFloat: return "const.float";
        case Opcode::ConstantBool: return "const.bool";
        case Opcode::ConstantString: return "const.string";
        case Opcode::ConstantNone: return "const.none";
        case Opcode::Load: return "load";
        case Opcode::Store: return "store";
        case Opcode::Add: return "add";
        case Opcode::Subtract: return "sub";
        case Opcode::Multiply: return "mul";
        case Opcode::Divide: return "div";
        case Opcode::Remainder: return "rem";
        case Opcode::Negate: return "neg";
        case Opcode::LogicalNot: return "not";
        case Opcode::LogicalAnd: return "and";
        case Opcode::LogicalOr: return "or";
        case Opcode::Equal: return "eq";
        case Opcode::NotEqual: return "ne";
        case Opcode::Less: return "lt";
        case Opcode::LessEqual: return "le";
        case Opcode::Greater: return "gt";
        case Opcode::GreaterEqual: return "ge";
        case Opcode::Call: return "call";
        case Opcode::Log: return "log";
        case Opcode::Jump: return "jump";
        case Opcode::Branch: return "branch";
        case Opcode::Return: return "return";
    }
    return "unknown";
}

std::string print(const Module& module) {
    std::ostringstream output;
    for (const auto& function : module.functions) {
        output << "func @" << function.name << '(';
        for (std::size_t i = 0; i < function.parameters.size(); ++i) {
            if (i) output << ", ";
            output << '%' << function.parameters[i].value << ' ' << function.parameters[i].name
                   << ": " << toString(function.parameters[i].type);
        }
        output << ") -> " << toString(function.returnType) << " {\n";
        for (const auto& block : function.blocks) {
            output << block.name << ":\n";
            for (const auto& instruction : block.instructions) {
                output << "  ";
                if (instruction.result) output << '%' << instruction.result << " = ";
                output << toString(instruction.opcode);
                if (!instruction.text.empty()) output << " \"" << instruction.text << '"';
                if (instruction.opcode == Opcode::ConstantInt ||
                    instruction.opcode == Opcode::ConstantBool)
                    output << ' ' << instruction.integer;
                if (instruction.opcode == Opcode::ConstantFloat)
                    output << ' ' << std::setprecision(17) << instruction.floating;
                for (const auto operand : instruction.operands) output << " %" << operand;
                if (instruction.opcode == Opcode::Jump) output << " bb" << instruction.trueTarget;
                if (instruction.opcode == Opcode::Branch)
                    output << " bb" << instruction.trueTarget << " bb" << instruction.falseTarget;
                output << '\n';
            }
        }
        output << "}\n";
    }
    return output.str();
}

} // namespace swan::sil
