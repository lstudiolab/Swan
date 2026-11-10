# AST's 50 checkers — implementation ledger

AST.swan will own 50 explicit validation/checker passes. This ledger makes the count auditable. Names below define the intended coverage; a checker remains **planned** until its implementation and regression tests exist.

| # | Checker | Responsibility | Status |
|---:|---|---|---|
| 1 | ValidNodeKind | recognized AST node kinds | Planned |
| 2 | NodeArity | valid child count per node kind | Planned |
| 3 | RequiredChildren | required child nodes exist | Planned |
| 4 | SourceSpan | valid source ranges | Planned |
| 5 | SourceOrder | child/source ordering | Planned |
| 6 | IdentifierShape | valid identifier representation | Planned |
| 7 | LiteralPayload | valid literal payloads | Planned |
| 8 | StringEncoding | valid UTF-8 string literals | Planned |
| 9 | NumericLiteral | valid numeric literal forms | Planned |
| 10 | BooleanLiteral | valid boolean literal forms | Planned |
| 11 | NullLiteral | valid null literal forms | Planned |
| 12 | DuplicateDeclarations | duplicate declaration detection | Planned |
| 13 | DeclarationPlacement | legal declaration positions | Planned |
| 14 | FunctionShape | function node structure | Planned |
| 15 | ParameterShape | parameter node structure | Planned |
| 16 | ReturnShape | return node structure | Planned |
| 17 | BlockShape | block node structure | Planned |
| 18 | StatementShape | statement node structure | Planned |
| 19 | ExpressionShape | expression node structure | Planned |
| 20 | TypeNodeShape | type syntax node structure | Planned |
| 21 | GenericParameterShape | generic parameter structure | Planned |
| 22 | GenericArgumentShape | generic argument structure | Planned |
| 23 | ImportShape | import declaration structure | Planned |
| 24 | AttributeShape | attribute structure | Planned |
| 25 | ModifierShape | modifier structure | Planned |
| 26 | OperatorShape | operator and operand structure | Planned |
| 27 | CallShape | call callee and argument structure | Planned |
| 28 | MemberAccessShape | member access structure | Planned |
| 29 | IndexAccessShape | index access structure | Planned |
| 30 | AssignmentTarget | assignable target structure | Planned |
| 31 | PatternShape | pattern node structure | Planned |
| 32 | MatchArmShape | match arm structure | Planned |
| 33 | IfBranchShape | conditional branch structure | Planned |
| 34 | LoopShape | loop node structure | Planned |
| 35 | BreakContextShape | break node structure | Planned |
| 36 | ContinueContextShape | continue node structure | Planned |
| 37 | DeferShape | defer node structure | Planned |
| 38 | ClosureShape | closure capture structure | Planned |
| 39 | LambdaShape | lambda node structure | Planned |
| 40 | StructShape | struct declaration structure | Planned |
| 41 | EnumShape | enum declaration structure | Planned |
| 42 | TraitShape | trait declaration structure | Planned |
| 43 | ImplementationShape | implementation block structure | Planned |
| 44 | VisibilityShape | visibility modifier placement | Planned |
| 45 | AttributePlacement | legal attribute placement | Planned |
| 46 | TypeAnnotationPlacement | legal type annotation placement | Planned |
| 47 | MacroExpansionShape | expanded macro node structure | Planned |
| 48 | ErrorNodeContainment | parser error-node containment | Planned |
| 49 | ASTReachability | unreachable/orphan AST node detection | Planned |
