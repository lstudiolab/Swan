"""The complete Swan-to-C source transpiler in one file."""

from __future__ import annotations

import argparse
import json
import pathlib
import sys
from dataclasses import dataclass


VERSION = "0.3.0"
KEYWORDS = {
    "fn", "let", "var", "const", "struct", "if", "else", "while",
    "return", "break", "continue", "pass", "log", "true", "false",
    "none", "int", "float", "bool", "string", "void", "and", "or",
    "not", "import", "as", "for", "loop", "async", "await", "sync",
    "hold", "store", "operate", "clone", "get", "init", "deinit",
    "release", "enum", "class", "type", "public", "private", "export",
    "self", "case", "range",
}


class SwanError(Exception):
    """Base class for source diagnostics."""


class LexError(SwanError):
    pass


class ParseError(SwanError):
    pass


class SemanticError(SwanError):
    pass


class EmitError(SwanError):
    pass


@dataclass(frozen=True)
class Token:
    kind: str
    value: str
    line: int
    column: int


@dataclass
class Node:
    kind: str
    data: dict
    token: Token


class Lexer:
    """Tokenize Swan source while retaining source locations."""

    def __init__(self, source: str):
        self.source = source
        self.index = 0
        self.line = 1
        self.column = 1
        self.tokens = []

    def peek(self, offset=0):
        pos = self.index + offset
        return self.source[pos] if pos < len(self.source) else ""

    def advance(self):
        char = self.peek()
        if char:
            self.index += 1
            if char == "\n":
                self.line, self.column = self.line + 1, 1
            else:
                self.column += 1
        return char

    def tokenize(self):
        while self.peek():
            char = self.peek()
            if char in " \t\r":
                self.advance()
                continue
            if char == "\n":
                line, column = self.line, self.column
                self.advance()
                self.tokens.append(Token("NEWLINE", "\n", line, column))
                continue
            if char == "#":
                while self.peek() not in ("\n", ""):
                    self.advance()
                continue

            line, column = self.line, self.column
            if char.isalpha() or char == "_":
                start = self.index
                while self.peek().isalnum() or self.peek() == "_":
                    self.advance()
                value = self.source[start:self.index]
                kind = value.upper() if value in KEYWORDS else "IDENT"
                self.tokens.append(Token(kind, value, line, column))
                continue

            if char.isdigit():
                start = self.index
                while self.peek().isdigit():
                    self.advance()
                kind = "INT"
                if self.peek() == "." and self.peek(1).isdigit():
                    kind = "FLOAT"
                    self.advance()
                    while self.peek().isdigit():
                        self.advance()
                self.tokens.append(Token(kind, self.source[start:self.index], line, column))
                continue

            if char in ('"', "'"):
                quote = self.advance()
                value = []
                while self.peek() not in (quote, ""):
                    current = self.advance()
                    if current == "\\":
                        escaped = self.advance()
                        mapping = {
                            "n": "\n", "r": "\r", "t": "\t",
                            "\\": "\\", '"': '"', "'": "'",
                        }
                        if escaped not in mapping:
                            raise LexError(f"{line}:{column}: unknown escape sequence")
                        value.append(mapping[escaped])
                    else:
                        value.append(current)
                if self.peek() == "":
                    raise LexError(f"{line}:{column}: unterminated string")
                self.advance()
                self.tokens.append(Token("STRING", "".join(value), line, column))
                continue

            pair = char + self.peek(1)
            if pair in ("->", "==", "!=", "<=", ">=", "&&", "||"):
                self.advance()
                self.advance()
                self.tokens.append(Token(pair, pair, line, column))
                continue
            if char in "{}()[],:;.+-*/%=<>!":
                self.advance()
                self.tokens.append(Token(char, char, line, column))
                continue
            raise LexError(f"{line}:{column}: unexpected character {char!r}")

        self.tokens.append(Token("EOF", "", self.line, self.column))
        return self.tokens


class Parser:
    """Recursive-descent parser for Swan declarations and statements."""

    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0

    def peek(self, offset=0):
        return self.tokens[min(self.pos + offset, len(self.tokens) - 1)]

    def check(self, *kinds):
        return self.peek().kind in kinds

    def advance(self):
        token = self.peek()
        if token.kind != "EOF":
            self.pos += 1
        return token

    def match(self, *kinds):
        return self.advance() if self.check(*kinds) else None

    def expect(self, kind, message=None):
        token = self.match(kind)
        if token:
            return token
        current = self.peek()
        raise ParseError(
            f"{current.line}:{current.column}: {message or 'expected ' + kind}"
        )

    def separators(self):
        while self.match("NEWLINE", ";"):
            pass

    def parse(self):
        declarations = []
        self.separators()
        while not self.check("EOF"):
            declarations.append(self.declaration())
            self.separators()
        return declarations

    def declaration(self):
        if self.match("FN"):
            return self.function(self.tokens[self.pos - 1])
        if self.match("STRUCT"):
            return self.struct(self.tokens[self.pos - 1])
        if self.check("LET", "VAR", "CONST"):
            return self.variable()
        if self.match("IMPORT"):
            start = self.tokens[self.pos - 1]
            module = self.advance()
            if module.kind not in ("IDENT", "STRING"):
                raise ParseError(
                    f"{module.line}:{module.column}: expected module name"
                )
            alias = self.match("AS")
            alias = self.expect("IDENT").value if alias else None
            return Node("import", {"module": module.value, "alias": alias}, start)
        return self.statement()

    def type_name(self):
        token = self.advance()
        if token.kind not in ("IDENT", "INT", "FLOAT", "BOOL", "STRING", "VOID"):
            raise ParseError(f"{token.line}:{token.column}: expected type name")
        return token.value

    def function(self, start):
        name = self.expect("IDENT", "expected function name").value
        self.expect("(")
        params = []
        if not self.check(")"):
            while True:
                param_name = self.expect("IDENT", "expected parameter name").value
                param_type = self.type_name() if self.match(":") else None
                params.append((param_name, param_type))
                if not self.match(","):
                    break
        self.expect(")")
        result_type = self.type_name() if self.match("->") else "void"
        return Node(
            "function",
            {
                "name": name,
                "params": params,
                "return_type": result_type,
                "body": self.block(),
            },
            start,
        )

    def struct(self, start):
        name = self.expect("IDENT", "expected structure name").value
        self.expect("{")
        fields = []
        self.separators()
        while not self.check("}", "EOF"):
            self.match("VAR", "LET")
            field_name = self.expect("IDENT", "expected field name").value
            self.expect(":")
            fields.append((field_name, self.type_name()))
            self.separators()
        self.expect("}")
        return Node("struct", {"name": name, "fields": fields}, start)

    def variable(self):
        start = self.advance()
        name = self.expect("IDENT", "expected variable name").value
        typ = self.type_name() if self.match(":") else None
        value = self.expression() if self.match("=") else None
        if start.kind in ("LET", "CONST") and value is None:
            raise ParseError(
                f"{start.line}:{start.column}: immutable declaration requires a value"
            )
        return Node(
            "variable",
            {"name": name, "type": typ, "value": value, "mutable": start.kind == "VAR"},
            start,
        )

    def block(self):
        self.expect("{", "expected '{' to begin block")
        body = []
        self.separators()
        while not self.check("}", "EOF"):
            before = self.pos
            body.append(self.declaration())
            self.separators()
            if self.pos == before:
                token = self.peek()
                raise ParseError(
                    f"{token.line}:{token.column}: parser made no progress"
                )
        self.expect("}", "expected '}' to close block")
        return body

    def statement(self):
        if self.match("IF"):
            start = self.tokens[self.pos - 1]
            condition = self.expression()
            yes = self.block()
            no = []
            if self.match("ELSE"):
                if self.match("IF"):
                    nested = self.tokens[self.pos - 1]
                    nested_condition = self.expression()
                    nested_body = self.block()
                    no = [Node(
                        "if",
                        {"condition": nested_condition, "then": nested_body, "else": []},
                        nested,
                    )]
                else:
                    no = self.block()
            return Node(
                "if", {"condition": condition, "then": yes, "else": no}, start
            )

        if self.match("WHILE"):
            start = self.tokens[self.pos - 1]
            condition = self.expression()
            return Node("while", {"condition": condition, "body": self.block()}, start)

        if self.match("RETURN"):
            start = self.tokens[self.pos - 1]
            value = None if self.check("}", "NEWLINE", ";", "EOF") else self.expression()
            return Node("return", {"value": value}, start)

        if self.match("BREAK", "CONTINUE", "PASS"):
            token = self.tokens[self.pos - 1]
            return Node(token.value, {}, token)

        if self.match("LOG"):
            token = self.tokens[self.pos - 1]
            return Node("log", {"value": self.expression()}, token)

        expression = self.expression()
        return Node("expression", {"value": expression}, expression.token)

    def expression(self, minimum=1):
        left = self.prefix()
        precedence = {
            "=": 1, "OR": 2, "||": 2, "AND": 3, "&&": 3,
            "==": 4, "!=": 4, "<": 5, "<=": 5, ">": 5, ">=": 5,
            "+": 6, "-": 6, "*": 7, "/": 7, "%": 7,
        }
        while precedence.get(self.peek().kind, 0) >= minimum:
            operator = self.advance()
            level = precedence[operator.kind]
            right = self.expression(level if operator.kind == "=" else level + 1)
            left = Node(
                "binary",
                {"left": left, "op": operator.value, "right": right},
                operator,
            )
        return left

    def prefix(self):
        if self.match("!", "-", "NOT"):
            operator = self.tokens[self.pos - 1]
            return Node(
                "unary",
                {"op": operator.value, "value": self.expression(8)},
                operator,
            )

        token = self.advance()
        if token.kind in ("INT", "FLOAT", "STRING", "TRUE", "FALSE", "NONE"):
            value = {"TRUE": "true", "FALSE": "false", "NONE": "NULL"}.get(
                token.kind, token.value
            )
            node = Node(
                "literal", {"value": value, "literal_type": token.kind}, token
            )
        elif token.kind == "IDENT":
            node = Node("name", {"name": token.value}, token)
        elif token.kind == "(":
            node = self.expression()
            self.expect(")", "expected ')' after expression")
        else:
            raise ParseError(f"{token.line}:{token.column}: expected expression")

        while True:
            if self.match("("):
                args = []
                if not self.check(")"):
                    while True:
                        args.append(self.expression())
                        if not self.match(","):
                            break
                self.expect(")", "expected ')' after arguments")
                node = Node("call", {"callee": node, "args": args}, token)
                continue
            if self.match("."):
                member = self.expect("IDENT", "expected member name")
                node = Node("member", {"base": node, "name": member.value}, member)
                continue
            break
        return node


NUMERIC_TYPES = {"int", "float"}
BUILTIN_TYPES = {"int", "float", "bool", "string", "void"}


class Analyzer:
    """Validate declarations, names, mutability, calls, and expression types."""

    def __init__(self, nodes):
        self.nodes = nodes
        self.functions = {}
        self.types = set(BUILTIN_TYPES)

    def fail(self, node, message):
        raise SemanticError(f"{node.token.line}:{node.token.column}: {message}")

    def analyze(self):
        for node in self.nodes:
            if node.kind == "struct":
                name = node.data["name"]
                if name in self.types:
                    self.fail(node, f"type '{name}' is already defined")
                self.types.add(name)

        for node in self.nodes:
            if node.kind == "function":
                name = node.data["name"]
                if name in self.functions:
                    self.fail(node, f"function '{name}' is already declared")
                self.functions[name] = node

        for node in self.nodes:
            if node.kind == "struct":
                for field_name, typ in node.data["fields"]:
                    if typ not in self.types:
                        self.fail(node, f"unknown field type '{typ}' for '{field_name}'")
            elif node.kind == "function":
                self.function(node)
            else:
                self.fail(node, f"unsupported top-level declaration '{node.kind}'")

    def function(self, node):
        self.scopes = [{}]
        self.return_type = node.data["return_type"]
        if self.return_type not in self.types:
            self.fail(node, f"unknown return type '{self.return_type}'")
        for name, typ in node.data["params"]:
            if typ is None:
                self.fail(node, f"parameter '{name}' requires a type")
            if typ not in self.types:
                self.fail(node, f"unknown parameter type '{typ}'")
            if name in self.scopes[-1]:
                self.fail(node, f"parameter '{name}' is already declared")
            self.scopes[-1][name] = (typ, False)
        for statement in node.data["body"]:
            self.statement(statement)

    def lookup(self, name):
        for scope in reversed(self.scopes):
            if name in scope:
                return scope[name]
        return None

    @staticmethod
    def compatible(expected, actual):
        return expected == actual or (expected == "float" and actual == "int")

    def statement(self, node):
        data, kind = node.data, node.kind
        if kind == "variable":
            if data["name"] in self.scopes[-1]:
                self.fail(node, f"'{data['name']}' is already declared in this scope")
            actual = self.expression(data["value"]) if data["value"] else None
            typ = data["type"] or actual
            if typ is None:
                self.fail(node, f"cannot infer type for '{data['name']}'")
            if typ not in self.types:
                self.fail(node, f"unknown type '{typ}'")
            if actual and not self.compatible(typ, actual):
                self.fail(
                    node,
                    f"cannot assign {actual} to {typ} variable '{data['name']}'",
                )
            self.scopes[-1][data["name"]] = (typ, data["mutable"])
        elif kind == "log":
            typ = self.expression(data["value"])
            if typ not in ("string", "int", "float", "bool"):
                self.fail(node, "log expects a string, number, or boolean")
        elif kind == "return":
            actual = self.expression(data["value"]) if data["value"] else "void"
            if not self.compatible(self.return_type, actual):
                self.fail(
                    node,
                    f"function returns {self.return_type}, but this return has type {actual}",
                )
        elif kind == "expression":
            self.expression(data["value"])
        elif kind == "if":
            if self.expression(data["condition"]) != "bool":
                self.fail(node, "if condition must be bool")
            self.scopes.append({})
            for child in data["then"]:
                self.statement(child)
            self.scopes.pop()
            self.scopes.append({})
            for child in data["else"]:
                self.statement(child)
            self.scopes.pop()
        elif kind == "while":
            if self.expression(data["condition"]) != "bool":
                self.fail(node, "while condition must be bool")
            self.scopes.append({})
            for child in data["body"]:
                self.statement(child)
            self.scopes.pop()
        elif kind in ("break", "continue", "pass"):
            return
        else:
            self.fail(node, f"unsupported statement '{kind}'")

    def expression(self, node):
        data, kind = node.data, node.kind
        if kind == "literal":
            return {
                "INT": "int", "FLOAT": "float", "STRING": "string",
                "TRUE": "bool", "FALSE": "bool",
            }.get(data["literal_type"], "none")
        if kind == "name":
            found = self.lookup(data["name"])
            if found is None:
                self.fail(node, f"unknown name '{data['name']}'")
            return found[0]
        if kind == "unary":
            typ = self.expression(data["value"])
            if data["op"] in ("!", "not"):
                if typ != "bool":
                    self.fail(node, "logical not requires a bool")
                return "bool"
            if typ not in NUMERIC_TYPES:
                self.fail(node, "numeric negation requires a number")
            return typ
        if kind == "binary":
            operator = data["op"]
            left = self.expression(data["left"])
            if operator == "=":
                target = data["left"]
                if target.kind != "name":
                    self.fail(node, "assignment target must be a variable")
                found = self.lookup(target.data["name"])
                if found is None:
                    self.fail(target, f"unknown variable '{target.data['name']}'")
                if not found[1]:
                    self.fail(target, f"cannot assign to immutable value '{target.data['name']}'")
                right = self.expression(data["right"])
                if not self.compatible(found[0], right):
                    self.fail(node, f"cannot assign {right} to {found[0]}")
                return found[0]

            right = self.expression(data["right"])
            if operator in ("and", "or", "&&", "||"):
                if left != "bool" or right != "bool":
                    self.fail(node, "logical operators require bool operands")
                return "bool"
            if operator in ("==", "!=", "<", ">", "<=", ">="):
                if left != right and not (
                    left in NUMERIC_TYPES and right in NUMERIC_TYPES
                ):
                    self.fail(node, "comparison operands have incompatible types")
                return "bool"
            if left not in NUMERIC_TYPES or right not in NUMERIC_TYPES:
                self.fail(node, "arithmetic operators require numeric operands")
            return "float" if "float" in (left, right) or operator == "/" else "int"
        if kind == "call":
            callee = data["callee"]
            if callee.kind != "name":
                self.fail(node, "only direct function calls are supported")
            name = callee.data["name"]
            target = self.functions.get(name)
            if target is None:
                self.fail(node, f"unknown function '{name}'")
            expected = target.data["params"]
            if len(expected) != len(data["args"]):
                self.fail(
                    node,
                    f"function '{name}' expects {len(expected)} arguments, got {len(data['args'])}",
                )
            for argument, (_, typ) in zip(data["args"], expected):
                actual = self.expression(argument)
                if not self.compatible(typ, actual):
                    self.fail(argument, f"argument has type {actual}; expected {typ}")
            return target.data["return_type"]
        if kind == "member":
            self.fail(node, "member access is not supported yet")
        self.fail(node, f"unsupported expression '{kind}'")


class CEmitter:
    """Emit C source text. This class never invokes a C compiler."""

    TYPE_MAP = {
        "int": "int", "float": "double", "bool": "bool",
        "string": "const char *", "void": "void",
    }

    def __init__(self):
        self.lines = []
        self.indent = 0
        self.symbols = {}
        self.functions = {}
        self.struct_types = set()

    def line(self, text=""):
        self.lines.append(("    " * self.indent + text) if text else "")

    def ctype(self, typ):
        if typ in self.TYPE_MAP:
            return self.TYPE_MAP[typ]
        if typ in self.struct_types:
            return typ
        raise EmitError(f"unsupported type '{typ}'")

    def emit(self, nodes):
        self.lines = [
            "#include <stdbool.h>", "#include <stdio.h>", "#include <stddef.h>", ""
        ]
        functions = [node for node in nodes if node.kind == "function"]
        self.struct_types = {
            node.data["name"] for node in nodes if node.kind == "struct"
        }
        self.functions = {
            node.data["name"]: node.data["return_type"] for node in functions
        }

        for node in nodes:
            if node.kind == "struct":
                self.line(f"typedef struct {node.data['name']} {{")
                self.indent += 1
                for name, typ in node.data["fields"]:
                    self.line(f"{self.ctype(typ)} {name};")
                self.indent -= 1
                self.line(f"}} {node.data['name']};")
                self.line()

        for node in functions:
            params = []
            for name, typ in node.data["params"]:
                if typ is None:
                    raise EmitError(
                        f"{node.token.line}:{node.token.column}: parameter '{name}' requires a type"
                    )
                params.append(f"{self.ctype(typ)} {name}")
            self.line(
                f"{self.ctype(node.data['return_type'])} {node.data['name']}("
                f"{', '.join(params) if params else 'void'});"
            )
        if functions:
            self.line()

        for node in nodes:
            if node.kind == "function":
                self.function(node)
            elif node.kind != "struct":
                raise EmitError(
                    f"{node.token.line}:{node.token.column}: unsupported top-level declaration '{node.kind}'"
                )
        return "\n".join(self.lines).rstrip() + "\n"

    def function(self, node):
        data = node.data
        params = []
        self.symbols = {}
        for name, typ in data["params"]:
            if typ is None:
                raise EmitError(
                    f"{node.token.line}:{node.token.column}: parameter '{name}' requires a type"
                )
            params.append(f"{self.ctype(typ)} {name}")
            self.symbols[name] = typ

        args = ", ".join(params) if params else "void"
        self.line(
            f"{self.ctype(data['return_type'])} {data['name']}({args}) {{"
        )
        self.indent += 1
        for child in data["body"]:
            self.statement(child)
        self.indent -= 1
        self.line("}")
        self.line()

    def infer(self, node):
        if node is None:
            return None
        if node.kind == "literal":
            return {
                "INT": "int", "FLOAT": "float", "STRING": "string",
                "TRUE": "bool", "FALSE": "bool",
            }.get(node.data["literal_type"])
        if node.kind == "name":
            return self.symbols.get(node.data["name"])
        if node.kind == "call" and node.data["callee"].kind == "name":
            return self.functions.get(node.data["callee"].data["name"])
        if node.kind == "binary":
            operator = node.data["op"]
            if operator in ("==", "!=", "<", ">", "<=", ">=", "and", "or", "&&", "||"):
                return "bool"
            left = self.infer(node.data["left"])
            right = self.infer(node.data["right"])
            if operator == "/" or "float" in (left, right):
                return "float"
            return left
        return None

    def statement(self, node):
        data, kind = node.data, node.kind
        if kind == "variable":
            inferred = data["type"] or self.infer(data["value"])
            if inferred is None:
                raise EmitError(
                    f"{node.token.line}:{node.token.column}: cannot infer type for '{data['name']}'"
                )
            typ = self.ctype(inferred)
            self.symbols[data["name"]] = inferred
            qualifier = "" if data["mutable"] else "const "
            initializer = f" = {self.expr(data['value'])}" if data["value"] else ""
            self.line(f"{qualifier}{typ} {data['name']}{initializer};")
        elif kind == "log":
            value = data["value"]
            typ = self.infer(value)
            if typ == "string":
                self.line(f"puts({self.expr(value)});")
            elif typ == "float":
                self.line(f'printf("%g\\n", (double)({self.expr(value)}));')
            elif typ in ("int", "bool"):
                self.line(f'printf("%d\\n", (int)({self.expr(value)}));')
            else:
                raise EmitError(
                    f"{node.token.line}:{node.token.column}: log requires a string, number, or boolean"
                )
        elif kind == "return":
            self.line(
                "return;" if data["value"] is None
                else f"return {self.expr(data['value'])};"
            )
        elif kind == "expression":
            self.line(self.expr(data["value"]) + ";")
        elif kind == "if":
            self.line(f"if ({self.expr(data['condition'])}) {{")
            self.indent += 1
            for child in data["then"]:
                self.statement(child)
            self.indent -= 1
            if data["else"]:
                self.line("} else {")
                self.indent += 1
                for child in data["else"]:
                    self.statement(child)
                self.indent -= 1
            self.line("}")
        elif kind == "while":
            self.line(f"while ({self.expr(data['condition'])}) {{")
            self.indent += 1
            for child in data["body"]:
                self.statement(child)
            self.indent -= 1
            self.line("}")
        elif kind in ("break", "continue"):
            self.line(kind + ";")
        elif kind == "pass":
            self.line(";")
        else:
            raise EmitError(
                f"{node.token.line}:{node.token.column}: unsupported statement '{kind}'"
            )

    def expr(self, node):
        data, kind = node.data, node.kind
        if kind == "literal":
            return (
                json.dumps(data["value"], ensure_ascii=False)
                if data["literal_type"] == "STRING" else data["value"]
            )
        if kind == "name":
            return data["name"]
        if kind == "unary":
            operator = "!" if data["op"] in ("not", "!") else data["op"]
            return operator + self.expr(data["value"])
        if kind == "binary":
            left = self.expr(data["left"])
            right = self.expr(data["right"])
            if data["op"] == "/":
                return f"((double)({left}) / (double)({right}))"
            operator = {"and": "&&", "or": "||"}.get(data["op"], data["op"])
            return f"({left} {operator} {right})"
        if kind == "call":
            return (
                f"{self.expr(data['callee'])}("
                f"{', '.join(self.expr(arg) for arg in data['args'])})"
            )
        if kind == "member":
            return f"{self.expr(data['base'])}.{data['name']}"
        raise EmitError(
            f"{node.token.line}:{node.token.column}: unsupported expression '{kind}'"
        )


def transpile(source: str) -> str:
    """Transpile Swan source text into C source text."""
    nodes = Parser(Lexer(source).tokenize()).parse()
    Analyzer(nodes).analyze()
    return CEmitter().emit(nodes)


def main(argv=None):
    command = argparse.ArgumentParser(
        prog="swan", description="Transpile Swan source into C source"
    )
    command.add_argument("source", help="input .swan file")
    command.add_argument("-o", "--output", help="output .c file; defaults to stdout")
    command.add_argument("--version", action="version", version=f"Swan {VERSION}")
    args = command.parse_args(argv)

    try:
        source_path = pathlib.Path(args.source)
        if source_path.suffix.lower() != ".swan":
            raise ValueError("input file must use the .swan extension")
        source = source_path.read_text(encoding="utf-8")
        result = transpile(source)

        if args.output:
            output_path = pathlib.Path(args.output)
            if output_path.suffix.lower() != ".c":
                raise ValueError("output file must use the .c extension")
            output_path.write_text(result, encoding="utf-8", newline="\n")
        else:
            sys.stdout.write(result)
        return 0
    except (OSError, UnicodeError, ValueError, SwanError) as error:
        print(f"swan: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
