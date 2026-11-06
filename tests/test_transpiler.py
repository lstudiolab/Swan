import unittest

from swan import (
    Analyzer, CEmitter, Lexer, ParseError, Parser, LexError, SemanticError
)


class TranspilerTests(unittest.TestCase):
    def transpile(self, source):
        nodes = Parser(Lexer(source).tokenize()).parse()
        Analyzer(nodes).analyze()
        return CEmitter().emit(nodes)

    def test_brace_blocks_and_log(self):
        result = self.transpile('fn main() -> int { log "Hello"; return 0; }')
        self.assertIn("int main(void) {", result)
        self.assertIn('puts("Hello");', result)

    def test_nested_control_flow(self):
        result = self.transpile(
            "fn main() -> int { var x: int = 0; "
            "while x < 2 { if x == 1 { log x; } else { log 0; } "
            "x = x + 1; } return 0; }"
        )
        self.assertIn("while ((x < 2)) {", result)
        self.assertIn("} else {", result)

    def test_float_promotion_and_division(self):
        result = self.transpile(
            "fn main() -> int { log 1 + 2.5; log 5 / 2; return 0; }"
        )
        self.assertIn('printf("%g\\n", (double)((1 + 2.5)));', result)
        self.assertIn("((double)(5) / (double)(2))", result)

    def test_function_calls(self):
        result = self.transpile(
            "fn answer() -> int { return 42; } "
            "fn main() -> int { log answer(); return 0; }"
        )
        self.assertIn("int answer(void);", result)
        self.assertIn("answer()", result)

    def test_missing_closing_brace(self):
        with self.assertRaises(ParseError):
            self.transpile("fn main() { return 0;")

    def test_bad_character(self):
        with self.assertRaises(LexError):
            self.transpile("fn main() { @; }")

    def test_unknown_type(self):
        with self.assertRaises(SemanticError):
            self.transpile("fn main() -> Widget { return 0; }")

    def test_unknown_name(self):
        with self.assertRaises(SemanticError):
            self.transpile("fn main() -> int { log missing; return 0; }")

    def test_immutable_assignment(self):
        with self.assertRaises(SemanticError):
            self.transpile(
                "fn main() -> int { let value = 1; value = 2; return 0; }"
            )

    def test_wrong_argument_count(self):
        with self.assertRaises(SemanticError):
            self.transpile(
                "fn add(a: int, b: int) -> int { return a + b; } "
                "fn main() -> int { return add(1); }"
            )


if __name__ == "__main__":
    unittest.main()
