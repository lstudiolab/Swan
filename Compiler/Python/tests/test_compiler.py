import unittest
from swan_compiler.lexer import Lexer, LexError
from swan_compiler.parser import Parser, ParseError
from swan_compiler.emitter import CEmitter, EmitError
from swan_compiler.sema import Analyzer, SemanticError

class CompilerTests(unittest.TestCase):
    def compile(self, source):
        nodes=Parser(Lexer(source).tokenize()).parse()
        Analyzer(nodes).analyze()
        return CEmitter().emit(nodes)
    def test_brace_blocks_and_log(self):
        result=self.compile('fn main() -> int { log "Hello"; return 0; }')
        self.assertIn('int main(void) {', result)
        self.assertIn('puts("Hello");', result)
    def test_nested_control_flow(self):
        result=self.compile('fn main() -> int { var x: int = 0; while x < 2 { if x == 1 { log x; } else { log 0; } x = x + 1; } return 0; }')
        self.assertIn('while ((x < 2)) {', result)
        self.assertIn('} else {', result)
    def test_function_calls(self):
        result=self.compile('fn answer() -> int { return 42; } fn main() -> int { log answer(); return 0; }')
        self.assertIn('int answer(void);', result)
        self.assertIn('answer()', result)
    def test_missing_closing_brace(self):
        with self.assertRaises(ParseError): self.compile('fn main() { return 0;')
    def test_bad_character(self):
        with self.assertRaises(LexError): self.compile('fn main() { @; }')
    def test_unknown_type(self):
        with self.assertRaises(EmitError): self.compile('fn main() -> Widget { return 0; }')
    def test_unknown_name(self):
        with self.assertRaises(SemanticError): self.compile('fn main() -> int { log missing; return 0; }')
    def test_immutable_assignment(self):
        with self.assertRaises(SemanticError): self.compile('fn main() -> int { let value = 1; value = 2; return 0; }')
    def test_wrong_argument_count(self):
        with self.assertRaises(SemanticError): self.compile('fn add(a: int, b: int) -> int { return a + b; } fn main() -> int { return add(1); }')

if __name__ == '__main__': unittest.main()
