"""Command-line interface for Swan's Python compiler."""
import argparse
import pathlib
import sys
from .lexer import Lexer, LexError
from .parser import Parser, ParseError
from .emitter import CEmitter, EmitError

VERSION="0.2.0"
def main(argv=None):
    cli=argparse.ArgumentParser(prog="swan",description="Swan source-to-C transpiler")
    cli.add_argument("source",help="input .swan file")
    cli.add_argument("-o","--output",help="output .c file; defaults to stdout")
    cli.add_argument("--version",action="version",version=f"Swan {VERSION}")
    args=cli.parse_args(argv)
    try:
        path=pathlib.Path(args.source)
        if path.suffix.lower()!=".swan": raise ValueError("input file must use the .swan extension")
        source=path.read_text(encoding="utf-8")
        result=CEmitter().emit(Parser(Lexer(source).tokenize()).parse())
        if args.output:
            target=pathlib.Path(args.output)
            if target.suffix.lower()!=".c": raise ValueError("output file must use the .c extension")
            target.write_text(result,encoding="utf-8",newline="\\n")
        else: sys.stdout.write(result)
        return 0
    except (OSError,UnicodeError,ValueError,LexError,ParseError,EmitError) as error:
        print(f"swan: {error}",file=sys.stderr); return 1
if __name__=="__main__": raise SystemExit(main())
