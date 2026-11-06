"""Basic name, call, mutability, and type validation for Swan."""
class SemanticError(Exception): pass
NUMERIC={"int","float"}
BUILTIN_TYPES={"int","float","bool","string","void"}

class Analyzer:
    def __init__(self,nodes):
        self.nodes=nodes
        self.functions={}
        self.types=set(BUILTIN_TYPES)
    def fail(self,node,message):
        raise SemanticError(f"{node.token.line}:{node.token.column}: {message}")
    def analyze(self):
        for node in self.nodes:
            if node.kind=="struct":
                name=node.data["name"]
                if name in self.types: self.fail(node,f"type '{name}' is already defined")
                self.types.add(name)
        for node in self.nodes:
            if node.kind=="function":
                name=node.data["name"]
                if name in self.functions: self.fail(node,f"function '{name}' is already declared")
                self.functions[name]=node
        for node in self.nodes:
            if node.kind=="struct":
                for field,typ in node.data["fields"]:
                    if typ not in self.types: self.fail(node,f"unknown field type '{typ}' for '{field}'")
            elif node.kind=="function": self.function(node)
            else: self.fail(node,f"unsupported top-level declaration '{node.kind}'")
    def function(self,node):
        self.scopes=[{}]
        self.return_type=node.data["return_type"]
        if self.return_type not in self.types: self.fail(node,f"unknown return type '{self.return_type}'")
        for name,typ in node.data["params"]:
            if typ is None: self.fail(node,f"parameter '{name}' requires a type")
            if typ not in self.types: self.fail(node,f"unknown parameter type '{typ}'")
            if name in self.scopes[-1]: self.fail(node,f"parameter '{name}' is already declared")
            self.scopes[-1][name]=(typ,False)
        for statement in node.data["body"]: self.statement(statement)
    def lookup(self,name):
        for scope in reversed(self.scopes):
            if name in scope: return scope[name]
        return None
    def compatible(self,expected,actual):
        return expected==actual or (expected=="float" and actual=="int")
    def statement(self,node):
        d=node.data; k=node.kind
        if k=="variable":
            if d["name"] in self.scopes[-1]: self.fail(node,f"'{d['name']}' is already declared in this scope")
            actual=self.expression(d["value"]) if d["value"] else None
            typ=d["type"] or actual
            if typ is None: self.fail(node,f"cannot infer type for '{d['name']}'")
            if typ not in self.types: self.fail(node,f"unknown type '{typ}'")
            if actual and not self.compatible(typ,actual): self.fail(node,f"cannot assign {actual} to {typ} variable '{d['name']}'")
            self.scopes[-1][d["name"]]=(typ,d["mutable"])
        elif k=="log":
            typ=self.expression(d["value"])
            if typ not in ("string","int","float","bool"): self.fail(node,"log expects a string, number, or boolean")
        elif k=="return":
            actual=self.expression(d["value"]) if d["value"] else "void"
            if not self.compatible(self.return_type,actual): self.fail(node,f"function returns {self.return_type}, but this return has type {actual}")
        elif k=="expression": self.expression(d["value"])
        elif k=="if":
            if self.expression(d["condition"])!="bool": self.fail(node,"if condition must be bool")
            self.scopes.append({})
            for child in d["then"]: self.statement(child)
            self.scopes.pop()
            self.scopes.append({})
            for child in d["else"]: self.statement(child)
            self.scopes.pop()
        elif k=="while":
            if self.expression(d["condition"])!="bool": self.fail(node,"while condition must be bool")
            self.scopes.append({})
            for child in d["body"]: self.statement(child)
            self.scopes.pop()
        elif k in ("break","continue","pass"): return
        else: self.fail(node,f"unsupported statement '{k}'")
    def expression(self,node):
        d=node.data; k=node.kind
        if k=="literal":
            return {"INT":"int","FLOAT":"float","STRING":"string","TRUE":"bool","FALSE":"bool"}.get(d["literal_type"],"none")
        if k=="name":
            found=self.lookup(d["name"])
            if found is None: self.fail(node,f"unknown name '{d['name']}'")
            return found[0]
        if k=="unary":
            typ=self.expression(d["value"])
            if d["op"] in ("!","not"):
                if typ!="bool": self.fail(node,"logical not requires a bool")
                return "bool"
            if typ not in NUMERIC: self.fail(node,"numeric negation requires a number")
            return typ
        if k=="binary":
            op=d["op"]
            left=self.expression(d["left"])
            if op=="=":
                target=d["left"]
                if target.kind!="name": self.fail(node,"assignment target must be a variable")
                found=self.lookup(target.data["name"])
                if found is None: self.fail(target,f"unknown variable '{target.data['name']}'")
                if not found[1]: self.fail(target,f"cannot assign to immutable value '{target.data['name']}'")
                right=self.expression(d["right"])
                if not self.compatible(found[0],right): self.fail(node,f"cannot assign {right} to {found[0]}")
                return found[0]
            right=self.expression(d["right"])
            if op in ("and","or","&&","||"):
                if left!="bool" or right!="bool": self.fail(node,"logical operators require bool operands")
                return "bool"
            if op in ("==","!=","<",">","<=",">="):
                if left!=right and not (left in NUMERIC and right in NUMERIC): self.fail(node,"comparison operands have incompatible types")
                return "bool"
            if left not in NUMERIC or right not in NUMERIC: self.fail(node,"arithmetic operators require numeric operands")
            return "float" if "float" in (left,right) or op=="/" else "int"
        if k=="call":
            callee=d["callee"]
            if callee.kind!="name": self.fail(node,"only direct function calls are supported")
            name=callee.data["name"]
            target=self.functions.get(name)
            if target is None: self.fail(node,f"unknown function '{name}'")
            expected=target.data["params"]
            if len(expected)!=len(d["args"]): self.fail(node,f"function '{name}' expects {len(expected)} arguments, got {len(d['args'])}")
            for arg,(_,typ) in zip(d["args"],expected):
                actual=self.expression(arg)
                if not self.compatible(typ,actual): self.fail(arg,f"argument has type {actual}; expected {typ}")
            return target.data["return_type"]
        if k=="member": self.fail(node,"member access is not supported yet")
        self.fail(node,f"unsupported expression '{k}'")
