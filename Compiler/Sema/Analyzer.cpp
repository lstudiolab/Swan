#include "Swan/Sema/Analyzer.hpp"

#include <utility>

namespace swan {

SemanticAnalyzer::SemanticAnalyzer(const Module& module)
    : module_(module), currentScope_(&globalScope_) {
    diagnostics_.reserve(8);
}

const std::vector<SemaDiagnostic>& SemanticAnalyzer::diagnostics() const noexcept {
    return diagnostics_;
}

const Scope& SemanticAnalyzer::globalScope() const noexcept {
    return globalScope_;
}

bool SemanticAnalyzer::analyze() {
    diagnostics_.clear();
    predeclare();
    for (const auto& statement : module_.statements) {
        if (statement) analyzeStatement(*statement);
    }
    return diagnostics_.empty();
}

void SemanticAnalyzer::report(SourceLocation location, std::string code,
                              std::string message) {
    diagnostics_.emplace_back(location, std::move(code), std::move(message));
}

void SemanticAnalyzer::predeclare() {
    for (const auto& statement : module_.statements) {
        if (!statement) continue;

        if (const auto* function = dynamic_cast<const FunctionDecl*>(statement.get())) {
            Symbol symbol(function->name, SymbolKind::Function, Type::function());
            symbol.parameterCount = function->parameters.size();
            symbol.returnType = Type::unknown();
            if (!globalScope_.define(std::move(symbol))) {
                report(function->location, "S001",
                       "Duplicate declaration of '" + function->name + "'.");
            }
            continue;
        }

        if (const auto* structure = dynamic_cast<const StructDecl*>(statement.get())) {
            Symbol symbol(structure->name, SymbolKind::Structure,
                          Type::structure(structure->name));
            for (const auto& member : structure->members) {
                if (const auto* field = dynamic_cast<const VarDecl*>(member.get()))
                    symbol.fields.push_back(field->name);
            }
            if (!globalScope_.define(std::move(symbol))) {
                report(structure->location, "S001",
                       "Duplicate declaration of '" + structure->name + "'.");
            }
        }
    }

    // Resolve declared return types only after all structures have been predeclared.
    for (const auto& statement : module_.statements) {
        const auto* function = dynamic_cast<const FunctionDecl*>(statement.get());
        if (!function || !function->returnType) continue;
        if (auto* symbol = globalScope_.lookup(function->name))
            symbol->returnType = resolveType(*function->returnType, function->location);
    }

    const char* builtins[] = {"range", "self", "init"};
    for (const char* name : builtins) {
        if (!globalScope_.lookupLocal(name)) {
            globalScope_.define(Symbol(name, SymbolKind::Builtin, Type::unknown()));
        }
    }
}

bool SemanticAnalyzer::compatible(const Type& expected, const Type& actual) const noexcept {
    if (expected.isUnknown() || actual.isUnknown()) return true;
    if (expected == actual) return true;
    return expected.kind == TypeKind::Float && actual.kind == TypeKind::Int;
}

Type SemanticAnalyzer::resolveType(const std::string& name, SourceLocation location) {
    if (name == "int") return Type::integer();
    if (name == "float") return Type::floating();
    if (name == "string") return Type::string();
    if (name == "bool") return Type::boolean();
    if (name == "none" || name == "void") return Type::none();

    const auto* symbol = globalScope_.lookup(name);
    if (symbol && symbol->kind == SymbolKind::Structure) return symbol->type;

    report(location, "S002", "Unknown type '" + name + "'.");
    return Type::unknown();
}

void SemanticAnalyzer::analyzeBlock(const std::vector<StmtPtr>& statements) {
    Scope blockScope(currentScope_);
    Scope* previous = currentScope_;
    currentScope_ = &blockScope;
    for (const auto& statement : statements) {
        if (statement) analyzeStatement(*statement);
    }
    currentScope_ = previous;
}

void SemanticAnalyzer::analyzeVariable(const VarDecl& variable, bool field) {
    Type declaredType = Type::unknown();
    if (variable.typeName)
        declaredType = resolveType(*variable.typeName, variable.location);

    Type initializerType = Type::unknown();
    if (variable.initializer)
        initializerType = analyzeExpression(*variable.initializer);

    if (variable.typeName && variable.initializer &&
        !compatible(declaredType, initializerType)) {
        report(variable.location, "S003",
               "Cannot initialize '" + variable.name + "' of type '" +
               declaredType.toString() + "' with a value of type '" +
               initializerType.toString() + "'.");
    }

    if (field) return;

    const bool mutableValue = variable.declarationKind == TokenKind::Var;
    Type valueType = variable.typeName ? declaredType : initializerType;
    Symbol symbol(variable.name, SymbolKind::Variable, valueType, mutableValue);
    if (!currentScope_->define(std::move(symbol))) {
        report(variable.location, "S001",
               "Duplicate declaration of '" + variable.name + "' in this scope.");
    }
}

void SemanticAnalyzer::analyzeFunction(const FunctionDecl& function) {
    Scope functionScope(currentScope_);
    Scope* previousScope = currentScope_;
    const Type previousReturn = expectedReturnType_;
    const bool previousHasReturn = hasExpectedReturnType_;
    const bool previousInsideFunction = insideFunction_;
    const std::size_t previousLoopDepth = loopDepth_;

    currentScope_ = &functionScope;
    loopDepth_ = 0;
    insideFunction_ = true;
    hasExpectedReturnType_ = function.returnType.has_value();
    const auto* functionSymbol = globalScope_.lookup(function.name);
    expectedReturnType_ = functionSymbol ? functionSymbol->returnType : Type::unknown();

    for (const auto& parameter : function.parameters) {
        Symbol symbol(parameter, SymbolKind::Parameter, Type::unknown(), true);
        if (!currentScope_->define(std::move(symbol))) {
            report(function.location, "S004",
                   "Duplicate parameter name '" + parameter + "' in function '" +
                   function.name + "'.");
        }
    }

    for (const auto& statement : function.body) {
        if (statement) analyzeStatement(*statement);
    }

    currentScope_ = previousScope;
    expectedReturnType_ = previousReturn;
    hasExpectedReturnType_ = previousHasReturn;
    insideFunction_ = previousInsideFunction;
    loopDepth_ = previousLoopDepth;
}

void SemanticAnalyzer::analyzeStruct(const StructDecl& structure) {
    Scope structureScope(currentScope_);
    Scope* previous = currentScope_;
    currentScope_ = &structureScope;

    for (const auto& member : structure.members) {
        if (!member) continue;
        if (const auto* field = dynamic_cast<const VarDecl*>(member.get())) {
            analyzeVariable(*field, true);
        } else {
            report(member->location, "S005",
                   "Only stored fields are currently supported inside a struct.");
        }
    }

    currentScope_ = previous;
}

void SemanticAnalyzer::analyzeIf(const IfStmt& statement) {
    if (statement.condition) analyzeExpression(*statement.condition);
    analyzeBlock(statement.body);
    analyzeBlock(statement.elseBody);
}

void SemanticAnalyzer::analyzeWhile(const WhileStmt& statement) {
    if (statement.condition) analyzeExpression(*statement.condition);
    ++loopDepth_;
    analyzeBlock(statement.body);
    --loopDepth_;
}

void SemanticAnalyzer::analyzeFor(const ForStmt& statement) {
    if (statement.sequence) analyzeExpression(*statement.sequence);

    Scope loopScope(currentScope_);
    Scope* previous = currentScope_;
    currentScope_ = &loopScope;
    currentScope_->define(Symbol(statement.variable, SymbolKind::Variable,
                                 Type::unknown(), false));
    ++loopDepth_;
    for (const auto& child : statement.body) {
        if (child) analyzeStatement(*child);
    }
    --loopDepth_;
    currentScope_ = previous;
}

void SemanticAnalyzer::analyzeStatement(const Stmt& statement) {
    if (const auto* variable = dynamic_cast<const VarDecl*>(&statement)) {
        analyzeVariable(*variable);
        return;
    }
    if (const auto* function = dynamic_cast<const FunctionDecl*>(&statement)) {
        analyzeFunction(*function);
        return;
    }
    if (const auto* structure = dynamic_cast<const StructDecl*>(&statement)) {
        analyzeStruct(*structure);
        return;
    }
    if (const auto* expression = dynamic_cast<const ExprStmt*>(&statement)) {
        if (expression->expression) analyzeExpression(*expression->expression);
        return;
    }
    if (const auto* conditional = dynamic_cast<const IfStmt*>(&statement)) {
        analyzeIf(*conditional);
        return;
    }
    if (const auto* loop = dynamic_cast<const WhileStmt*>(&statement)) {
        analyzeWhile(*loop);
        return;
    }
    if (const auto* loop = dynamic_cast<const ForStmt*>(&statement)) {
        analyzeFor(*loop);
        return;
    }
    if (const auto* result = dynamic_cast<const ReturnStmt*>(&statement)) {
        if (!insideFunction_) {
            report(result->location, "S006", "'return' is only valid inside a function.");
        }
        Type actual = result->value ? analyzeExpression(*result->value) : Type::none();
        if (hasExpectedReturnType_ && !compatible(expectedReturnType_, actual)) {
            report(result->location, "S007",
                   "Function returns '" + actual.toString() +
                   "' but declares '" + expectedReturnType_.toString() + "'.");
        }
        return;
    }
    if (const auto* control = dynamic_cast<const ControlStmt*>(&statement)) {
        if (control->kind == TokenKind::Break || control->kind == TokenKind::Continue) {
            if (loopDepth_ == 0) {
                report(control->location, "S008",
                       "'" + std::string(tokenKindName(control->kind)) +
                       "' is only valid inside a loop.");
            }
        }
        return;
    }
    if (dynamic_cast<const ImportStmt*>(&statement)) {
        // Module loading and import visibility are resolved by a later compiler stage.
        return;
    }
}

Type SemanticAnalyzer::analyzeExpression(const Expr& expression) {
    if (const auto* literal = dynamic_cast<const LiteralExpr*>(&expression)) {
        switch (literal->token.kind) {
            case TokenKind::Integer: return Type::integer();
            case TokenKind::Float: return Type::floating();
            case TokenKind::String: return Type::string();
            case TokenKind::True:
            case TokenKind::False: return Type::boolean();
            case TokenKind::None: return Type::none();
            default: return Type::unknown();
        }
    }

    if (const auto* name = dynamic_cast<const NameExpr*>(&expression)) {
        const auto* symbol = currentScope_->lookup(name->name);
        if (!symbol) {
            report(name->location, "S009",
                   "Use of undeclared name '" + name->name + "'.");
            return Type::unknown();
        }
        return symbol->type;
    }

    if (const auto* unary = dynamic_cast<const UnaryExpr*>(&expression)) {
        const Type operand = unary->operand
            ? analyzeExpression(*unary->operand) : Type::unknown();
        switch (unary->op.kind) {
            case TokenKind::Minus:
                if (!operand.isUnknown() && !operand.isNumeric())
                    report(unary->location, "S010",
                           "Unary '-' requires a numeric operand.");
                return operand;
            case TokenKind::Not:
                if (!operand.isUnknown() && operand.kind != TypeKind::Bool)
                    report(unary->location, "S011",
                           "'not' requires a boolean operand.");
                return Type::boolean();
            case TokenKind::Log:
                return Type::none();
            case TokenKind::Await:
            case TokenKind::Get:
            case TokenKind::Hold:
            case TokenKind::Store:
            case TokenKind::Clone:
            case TokenKind::Release:
            case TokenKind::Operate:
            case TokenKind::Sync:
                return operand;
            default:
                return Type::unknown();
        }
    }

    if (const auto* binary = dynamic_cast<const BinaryExpr*>(&expression)) {
        const Type left = binary->left
            ? analyzeExpression(*binary->left) : Type::unknown();
        const Type right = binary->right
            ? analyzeExpression(*binary->right) : Type::unknown();

        if (binary->op.kind == TokenKind::Assign) {
            const auto* name = dynamic_cast<const NameExpr*>(binary->left.get());
            const auto* member = dynamic_cast<const MemberExpr*>(binary->left.get());
            if (!name && !member) {
                report(binary->location, "S012",
                       "The left side of an assignment must be a variable or member.");
            } else if (name) {
                auto* symbol = currentScope_->lookup(name->name);
                if (symbol && !symbol->mutableValue &&
                    symbol->kind != SymbolKind::Parameter) {
                    report(binary->location, "S013",
                           "Cannot assign to immutable value '" + name->name + "'.");
                }
                if (symbol && !compatible(symbol->type, right)) {
                    report(binary->location, "S003",
                           "Cannot assign a value of type '" + right.toString() +
                           "' to '" + name->name + "' of type '" +
                           symbol->type.toString() + "'.");
                }
            }
            return left;
        }

        switch (binary->op.kind) {
            case TokenKind::Plus:
                if (left.kind == TypeKind::String && right.kind == TypeKind::String)
                    return Type::string();
                if (left.isNumeric() && right.isNumeric())
                    return left.kind == TypeKind::Float || right.kind == TypeKind::Float
                        ? Type::floating() : Type::integer();
                if (!left.isUnknown() && !right.isUnknown())
                    report(binary->location, "S014",
                           "'+' requires two numbers or two strings.");
                return Type::unknown();
            case TokenKind::Minus:
            case TokenKind::Star:
            case TokenKind::Slash:
            case TokenKind::Percent:
                if ((!left.isNumeric() && !left.isUnknown()) ||
                    (!right.isNumeric() && !right.isUnknown())) {
                    report(binary->location, "S015",
                           "Arithmetic operators require numeric operands.");
                    return Type::unknown();
                }
                if (binary->op.kind == TokenKind::Slash ||
                    left.kind == TypeKind::Float || right.kind == TypeKind::Float)
                    return Type::floating();
                return Type::integer();
            case TokenKind::Less:
            case TokenKind::LessEqual:
            case TokenKind::Greater:
            case TokenKind::GreaterEqual:
                if (!left.isUnknown() && !right.isUnknown() &&
                    !(left.isNumeric() && right.isNumeric()) && left != right) {
                    report(binary->location, "S016",
                           "Comparison operands have incompatible types.");
                }
                return Type::boolean();
            case TokenKind::Equal:
            case TokenKind::NotEqual:
                if (!compatible(left, right) && !compatible(right, left))
                    report(binary->location, "S016",
                           "Equality operands have incompatible types.");
                return Type::boolean();
            case TokenKind::And:
            case TokenKind::Or:
                if ((!left.isUnknown() && left.kind != TypeKind::Bool) ||
                    (!right.isUnknown() && right.kind != TypeKind::Bool)) {
                    report(binary->location, "S017",
                           "Logical operators require boolean operands.");
                }
                return Type::boolean();
            default:
                return Type::unknown();
        }
    }

    if (const auto* member = dynamic_cast<const MemberExpr*>(&expression)) {
        if (member->base) analyzeExpression(*member->base);
        // Member lookup is intentionally deferred until Swan's member/access syntax
        // and module visibility rules are finalized.
        return Type::unknown();
    }

    if (const auto* call = dynamic_cast<const CallExpr*>(&expression)) {
        const auto* name = dynamic_cast<const NameExpr*>(call->callee.get());
        const Symbol* callee = name ? currentScope_->lookup(name->name) : nullptr;
        for (const auto& argument : call->arguments) {
            if (argument) analyzeExpression(*argument);
        }
        if (!callee) {
            if (name) {
                report(name->location, "S018",
                       "Cannot call undeclared function '" + name->name + "'.");
            } else if (call->callee) {
                analyzeExpression(*call->callee);
            }
            return Type::unknown();
        }
        if (callee->kind != SymbolKind::Function &&
            callee->kind != SymbolKind::Builtin) {
            report(call->location, "S019",
                   "Value '" + name->name + "' is not callable.");
            return Type::unknown();
        }
        if (callee->kind == SymbolKind::Function &&
            callee->parameterCount != call->arguments.size()) {
            report(call->location, "S020",
                   "Function '" + name->name + "' expects " +
                   std::to_string(callee->parameterCount) + " argument(s), but got " +
                   std::to_string(call->arguments.size()) + ".");
        }
        return callee->kind == SymbolKind::Function ? callee->returnType : Type::unknown();
    }

    if (const auto* array = dynamic_cast<const ArrayExpr*>(&expression)) {
        Type elementType = Type::unknown();
        for (const auto& element : array->elements) {
            if (!element) continue;
            const Type current = analyzeExpression(*element);
            if (elementType.isUnknown()) elementType = current;
            else if (!current.isUnknown() && !compatible(elementType, current) &&
                     !compatible(current, elementType)) {
                report(element->location, "S021",
                       "Array elements must have compatible types.");
            } else if (elementType.kind == TypeKind::Int &&
                       current.kind == TypeKind::Float) {
                elementType = Type::floating();
            }
        }
        return Type::array(elementType);
    }

    return Type::unknown();
}

} // namespace swan
