"""Tokenizer for Swan's brace-delimited syntax."""
from dataclasses import dataclass

@dataclass(frozen=True)
class Token:
    kind: str
    value: str
    line: int
    column: int

class LexError(Exception):
    pass

KEYWORDS = {"fn", "let", "var", "const", "struct", "if", "else", "while", "return", "break", "continue", "pass", "log", "true", "false", "none", "int", "float", "bool", "string", "void", "and", "or", "not", "import", "as"}

class Lexer:
    def __init__(self, source: str):
        self.source, self.index, self.line, self.column = source, 0, 1, 1
        self.tokens = []

    def peek(self, offset=0):
        pos = self.index + offset
        return self.source[pos] if pos < len(self.source) else "\\0"

    def advance(self):
        ch = self.peek()
        if ch != "\\0":
            self.index += 1
            if ch == "\\n": self.line, self.column = self.line + 1, 1
            else: self.column += 1
        return ch

    def tokenize(self):
        while self.peek() != "\\0":
            ch = self.peek()
            if ch in " \\t\\r": self.advance(); continue
            if ch == "\\n":
                line, col = self.line, self.column
                self.advance(); self.tokens.append(Token("NEWLINE", "\\n", line, col)); continue
            if ch == "#":
                while self.peek() not in ("\\n", "\\0"): self.advance()
                continue
            line, col = self.line, self.column
            if ch.isalpha() or ch == "_":
                start = self.index
                while self.peek().isalnum() or self.peek() == "_": self.advance()
                value = self.source[start:self.index]
                self.tokens.append(Token(value.upper() if value in KEYWORDS else "IDENT", value, line, col)); continue
            if ch.isdigit():
                start = self.index
                while self.peek().isdigit(): self.advance()
                kind = "INT"
                if self.peek() == "." and self.peek(1).isdigit():
                    kind = "FLOAT"; self.advance()
                    while self.peek().isdigit(): self.advance()
                self.tokens.append(Token(kind, self.source[start:self.index], line, col)); continue
            if ch in ('"', "'"):
                quote = self.advance(); value = []
                while self.peek() not in (quote, "\\0"):
                    current = self.advance()
                    if current == "\\\\":
                        escaped = self.advance()
                        mapping = {"n":"\\n", "r":"\\r", "t":"\\t", "\\\\":"\\\\", '"':'"', "'":"'"}
                        if escaped not in mapping: raise LexError(f"{line}:{col}: unknown escape sequence")
                        value.append(mapping[escaped])
                    else: value.append(current)
                if self.peek() == "\\0": raise LexError(f"{line}:{col}: unterminated string")
                self.advance(); self.tokens.append(Token("STRING", "".join(value), line, col)); continue
            pair = ch + self.peek(1)
            if pair in ("->", "==", "!=", "<=", ">=", "&&", "||"):
                self.advance(); self.advance(); self.tokens.append(Token(pair, pair, line, col)); continue
            if ch in "{}()[],:;.+-*/%=<>!":
                self.advance(); self.tokens.append(Token(ch, ch, line, col)); continue
            raise LexError(f"{line}:{col}: unexpected character {ch!r}")
        self.tokens.append(Token("EOF", "", self.line, self.column))
        return self.tokens
