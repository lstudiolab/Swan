"""C source emitter. This stage emits C text only; it never invokes a C compiler."""
import json

class EmitError(Exception): pass
TYPE_MAP={"int":"int","float":"double","bool":"bool","string":"const char *","void":"void"}

class CEmitter:
    def __init__(self):
        self.lines=[]; self.indent=0; self.symbols={}; self.functions={}; self.struct_types=set()
    def line(self,text=""):
        self.lines.append(("    "*self.indent+text) if text else "")
    def ctype(self,typ):
        if typ in TYPE_MAP: return TYPE_MAP[typ]
        if typ in self.struct_types: return typ
        raise EmitError(f"unsupported type '{typ}'")
    def emit(self,nodes):
        self.lines=["#include <stdbool.h>","#include <stdio.h>","#include <stddef.h>",""]
        functions=[node for node in nodes if node.kind=="function"]
        self.struct_types={node.data["name"] for node in nodes if node.kind=="struct"}
        self.functions={node.data["name"]:node.data["return_type"] for node in functions}
        for node in nodes:
            if node.kind=="struct":
                self.line(f"typedef struct {node.data['name']} {{"); self.indent+=1
                for name,typ in node.data["fields"]: self.line(f"{self.ctype(typ)} {name};")
                self.indent-=1; self.line(f"}} {node.data['name']};"); self.line()
        for node in functions:
            d=node.data; params=[]
            for name,typ in d["params"]:
                if typ is None: raise EmitError(f"{node.token.line}:{node.token.column}: parameter '{name}' requires a type")
                params.append(f"{self.ctype(typ)} {name}")
            self.line(f"{self.ctype(d['return_type'])} {d['name']}({', '.join(params) if params else 'void'});")
        if functions: self.line()
        for node in nodes:
            if node.kind=="function": self.function(node)
            elif node.kind!="struct": raise EmitError(f"{node.token.line}:{node.token.column}: unsupported top-level declaration '{node.kind}'")
        return "\n".join(self.lines).rstrip()+"\n"
    def function(self,node):
        d=node.data; params=[]; self.symbols={}
        for name,typ in d["params"]:
            if typ is None: raise EmitError(f"{node.token.line}:{node.token.column}: parameter '{name}' requires a type")
            params.append(f"{self.ctype(typ)} {name}"); self.symbols[name]=typ
        args=", ".join(params) if params else "void"
        self.line(f"{self.ctype(d['return_type'])} {d['name']}({args}) {{"); self.indent+=1
        for child in d["body"]: self.statement(child)
        self.indent-=1; self.line("}"); self.line()
    def infer(self,node):
        if node is None: return None
        if node.kind=="literal": return {"INT":"int","FLOAT":"float","STRING":"string","TRUE":"bool","FALSE":"bool"}.get(node.data["literal_type"])
        if node.kind=="name": return self.symbols.get(node.data["name"])
        if node.kind=="call" and node.data["callee"].kind=="name": return self.functions.get(node.data["callee"].data["name"])
        if node.kind=="binary": return "bool" if node.data["op"] in ("==","!=","<",">","<=",">=","and","or","&&","||") else self.infer(node.data["left"])
        return None
    def statement(self,node):
        d=node.data; k=node.kind
        if k=="variable":
            inferred=d["type"] or self.infer(d["value"])
            if inferred is None: raise EmitError(f"{node.token.line}:{node.token.column}: cannot infer type for '{d['name']}'")
            typ=self.ctype(inferred); self.symbols[d["name"]]=inferred
            qualifier="" if d["mutable"] else "const "
            init=f" = {self.expr(d['value'])}" if d["value"] else ""
            self.line(f"{qualifier}{typ} {d['name']}{init};")
        elif k=="log":
            value=d["value"]; typ=self.infer(value)
            if typ=="string": self.line(f"puts({self.expr(value)});")
            elif typ=="float": self.line(f'printf("%g\\n", (double)({self.expr(value)}));')
            elif typ in ("int","bool"): self.line(f'printf("%d\\n", (int)({self.expr(value)}));')
            else: raise EmitError(f"{node.token.line}:{node.token.column}: log currently requires a string, number, or boolean")
        elif k=="return": self.line("return;" if d["value"] is None else f"return {self.expr(d['value'])};")
        elif k=="expression": self.line(self.expr(d["value"])+";")
        elif k=="if":
            self.line(f"if ({self.expr(d['condition'])}) {{"); self.indent+=1
            for child in d["then"]: self.statement(child)
            self.indent-=1
            if d["else"]:
                self.line("} else {"); self.indent+=1
                for child in d["else"]: self.statement(child)
                self.indent-=1
            self.line("}")
        elif k=="while":
            self.line(f"while ({self.expr(d['condition'])}) {{"); self.indent+=1
            for child in d["body"]: self.statement(child)
            self.indent-=1; self.line("}")
        elif k in ("break","continue"): self.line(k+";")
        elif k=="pass": self.line(";")
        else: raise EmitError(f"{node.token.line}:{node.token.column}: unsupported statement '{k}'")
    def expr(self,node):
        d=node.data; k=node.kind
        if k=="literal": return json.dumps(d["value"],ensure_ascii=False) if d["literal_type"]=="STRING" else d["value"]
        if k=="name": return d["name"]
        if k=="unary": return ("!" if d["op"] in ("not","!") else d["op"])+self.expr(d["value"])
        if k=="binary": return f"({self.expr(d['left'])} { {'and':'&&','or':'||'}.get(d['op'],d['op']) } {self.expr(d['right'])})"
        if k=="call": return f"{self.expr(d['callee'])}({', '.join(self.expr(a) for a in d['args'])})"
        if k=="member": return f"{self.expr(d['base'])}.{d['name']}"
        raise EmitError(f"{node.token.line}:{node.token.column}: unsupported expression '{k}'")
