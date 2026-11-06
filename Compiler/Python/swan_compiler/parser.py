"""Recursive-descent parser and expression parser for Swan."""
from dataclasses import dataclass

class ParseError(Exception): pass

@dataclass
class Node:
    kind: str
    data: dict
    token: object

class Parser:
    def __init__(self, tokens): self.tokens, self.pos = tokens, 0
    def peek(self, n=0): return self.tokens[min(self.pos+n, len(self.tokens)-1)]
    def check(self, *kinds): return self.peek().kind in kinds
    def advance(self):
        token=self.peek()
        if token.kind != "EOF": self.pos += 1
        return token
    def match(self, *kinds): return self.advance() if self.check(*kinds) else None
    def expect(self, kind, message=None):
        token=self.match(kind)
        if token: return token
        t=self.peek(); raise ParseError(f"{t.line}:{t.column}: {message or 'expected '+kind}")
    def separators(self):
        while self.match("NEWLINE", ";"): pass
    def parse(self):
        result=[]; self.separators()
        while not self.check("EOF"):
            result.append(self.declaration()); self.separators()
        return result
    def declaration(self):
        if self.match("FN"): return self.function(self.tokens[self.pos-1])
        if self.match("STRUCT"): return self.struct(self.tokens[self.pos-1])
        if self.check("LET","VAR","CONST"): return self.variable()
        if self.match("IMPORT"):
            start=self.tokens[self.pos-1]; module=self.advance()
            if module.kind not in ("IDENT","STRING"): raise ParseError(f"{module.line}:{module.column}: expected module name")
            alias=self.match("AS"); alias=self.expect("IDENT").value if alias else None
            return Node("import",{"module":module.value,"alias":alias},start)
        return self.statement()
    def type_name(self):
        token=self.advance()
        if token.kind not in ("IDENT","INT","FLOAT","BOOL","STRING","VOID"): raise ParseError(f"{token.line}:{token.column}: expected type name")
        return token.value
    def function(self,start):
        name=self.expect("IDENT","expected function name").value; self.expect("("); params=[]
        if not self.check(")"):
            while True:
                pname=self.expect("IDENT","expected parameter name").value; typ=None
                if self.match(":"): typ=self.type_name()
                params.append((pname,typ))
                if not self.match(","): break
        self.expect(")"); result="void"
        if self.match("->"): result=self.type_name()
        return Node("function",{"name":name,"params":params,"return_type":result,"body":self.block()},start)
    def struct(self,start):
        name=self.expect("IDENT","expected structure name").value; self.expect("{"); fields=[]; self.separators()
        while not self.check("}","EOF"):
            self.match("VAR","LET"); field=self.expect("IDENT","expected field name").value; self.expect(":")
            fields.append((field,self.type_name())); self.separators()
        self.expect("}"); return Node("struct",{"name":name,"fields":fields},start)
    def variable(self):
        start=self.advance(); name=self.expect("IDENT","expected variable name").value; typ=None
        if self.match(":"): typ=self.type_name()
        value=self.expression() if self.match("=") else None
        if start.kind in ("LET","CONST") and value is None: raise ParseError(f"{start.line}:{start.column}: immutable declaration requires a value")
        return Node("variable",{"name":name,"type":typ,"value":value,"mutable":start.kind=="VAR"},start)
    def block(self):
        self.expect("{", "expected '{' to begin block"); body=[]; self.separators()
        while not self.check("}","EOF"):
            before=self.pos; body.append(self.declaration()); self.separators()
            if self.pos==before: raise ParseError(f"{self.peek().line}:{self.peek().column}: parser made no progress")
        self.expect("}","expected '}' to close block"); return body
    def statement(self):
        if self.match("IF"):
            start=self.tokens[self.pos-1]; cond=self.expression(); yes=self.block(); no=[]
            if self.match("ELSE"):
                if self.match("IF"):
                    nested=self.tokens[self.pos-1]; c=self.expression(); b=self.block(); no=[Node("if",{"condition":c,"then":b,"else":[]},nested)]
                else: no=self.block()
            return Node("if",{"condition":cond,"then":yes,"else":no},start)
        if self.match("WHILE"):
            start=self.tokens[self.pos-1]; cond=self.expression(); return Node("while",{"condition":cond,"body":self.block()},start)
        if self.match("RETURN"):
            start=self.tokens[self.pos-1]; value=None if self.check("}","NEWLINE",";","EOF") else self.expression()
            return Node("return",{"value":value},start)
        if self.match("BREAK","CONTINUE","PASS"):
            t=self.tokens[self.pos-1]; return Node(t.value,{},t)
        if self.match("LOG"):
            t=self.tokens[self.pos-1]; return Node("log",{"value":self.expression()},t)
        expr=self.expression(); return Node("expression",{"value":expr},expr.token)
    def expression(self, minimum=1):
        left=self.prefix()
        precedence={"=":1,"OR":2,"||":2,"AND":3,"&&":3,"==":4,"!=":4,"<":5,"<=":5,">":5,">=":5,"+":6,"-":6,"*":7,"/":7,"%":7}
        while precedence.get(self.peek().kind,0)>=minimum:
            op=self.advance(); level=precedence[op.kind]; right=self.expression(level if op.kind=="=" else level+1)
            left=Node("binary",{"left":left,"op":op.value,"right":right},op)
        return left
    def prefix(self):
        if self.match("!","-","NOT"):
            op=self.tokens[self.pos-1]; return Node("unary",{"op":op.value,"value":self.expression(8)},op)
        token=self.advance()
        if token.kind in ("INT","FLOAT","STRING","TRUE","FALSE","NONE"):
            value={"TRUE":"true","FALSE":"false","NONE":"NULL"}.get(token.kind,token.value)
            node=Node("literal",{"value":value,"literal_type":token.kind},token)
        elif token.kind=="IDENT": node=Node("name",{"name":token.value},token)
        elif token.kind=="(":
            node=self.expression(); self.expect(")","expected ')' after expression")
        else: raise ParseError(f"{token.line}:{token.column}: expected expression")
        while True:
            if self.match("("):
                args=[]
                if not self.check(")"):
                    while True:
                        args.append(self.expression())
                        if not self.match(","): break
                self.expect(")","expected ')' after arguments"); node=Node("call",{"callee":node,"args":args},token); continue
            if self.match("."):
                member=self.expect("IDENT","expected member name"); node=Node("member",{"base":node,"name":member.value},member); continue
            break
        return node
