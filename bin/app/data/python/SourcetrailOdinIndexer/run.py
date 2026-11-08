#!/usr/bin/env python3
"""Experimental Odin indexer for Sourcetrail.

This is deliberately small and self-contained: a lexer plus a declaration
scanner that emits Sourcetrail nodes and references through the same
SourcetrailDB/numbat binding used by the Python indexer.

It is a proof of concept. It does not type-check Odin and it does not try to
be a complete Odin parser. The parts that matter for navigation are covered:
packages, imports, structs/unions/enums/bit_sets/bit_fields, procedures,
fields, constants, calls, type usages and `using` embedding.
"""

import argparse
import json
import os
import sys
import time
from dataclasses import dataclass, field
from typing import Iterable, List, Optional, Sequence, Set, Tuple


# ---------------------------------------------------------------------------
# SourcetrailDB binding
# ---------------------------------------------------------------------------

def _python_indexer_dir() -> Optional[str]:
    env = os.environ.get("SOURCETRAIL_PYTHON_INDEXER")
    candidates = []
    if env:
        candidates.append(env)
    here = os.path.dirname(os.path.abspath(__file__))
    candidates.append(os.path.join(os.path.dirname(here), "SourcetrailPythonIndexer"))
    for candidate in candidates:
        if os.path.isfile(os.path.join(candidate, "sourcetraildb.py")):
            return candidate
    return None


def trace(message: str) -> None:
    path = os.environ.get("SOURCETRAIL_ODIN_TRACE")
    if not path:
        return
    try:
        with open(path, "a", encoding="utf-8") as handle:
            handle.write("%.3f %d %s\n" % (time.time(), os.getpid(), message))
    except OSError:
        pass


def load_srctrl():
    directory = _python_indexer_dir()
    if directory and directory not in sys.path:
        sys.path.insert(0, directory)
    import sourcetraildb as srctrl  # imported lazily so --dump works without numbat
    return srctrl


# ---------------------------------------------------------------------------
# Tokens
# ---------------------------------------------------------------------------

KEYWORDS = {
    "package", "import", "foreign", "proc", "struct", "union", "enum",
    "bit_set", "bit_field", "distinct", "map", "matrix", "using", "when",
    "where", "if", "else", "for", "in", "switch", "case", "defer", "return",
    "break", "continue", "fallthrough", "or_return", "or_else", "or_break",
    "or_continue", "nil", "true", "false", "cast", "transmute", "type_of",
    "type_info_of", "size_of", "align_of", "offset_of", "context",
}

MULTI_OPS = sorted([
    "::", ":=", "->", "..<", "..=", "...", "==", "!=", "<=", ">=", "&&",
    "||", "<<", ">>", "&~", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=",
], key=len, reverse=True)

BUILTIN_TYPES = {
    "int", "uint", "uintptr", "typeid", "rawptr", "string", "cstring", "bool",
    "b8", "b16", "b32", "b64", "rune", "any", "byte", "u8", "u16", "u32",
    "u64", "u128", "i8", "i16", "i32", "i64", "i128", "f16", "f32", "f64",
    "f128", "complex32", "complex64", "complex128", "quaternion64",
    "quaternion128", "quaternion256", "intle", "intbe", "uintle", "uintbe",
}


@dataclass
class Token:
    kind: str
    value: str
    line: int
    col: int
    end_line: int
    end_col: int


def lex(source: str) -> List[Token]:
    tokens: List[Token] = []
    i = 0
    line = 1
    col = 1
    n = len(source)

    def advance(k: int = 1) -> None:
        nonlocal i, line, col
        for _ in range(k):
            if i >= n:
                return
            if source[i] == "\n":
                line += 1
                col = 1
            else:
                col += 1
            i += 1

    def emit(kind: str, start_i: int, start_line: int, start_col: int) -> None:
        tokens.append(Token(kind, source[start_i:i], start_line, start_col, line, col))

    while i < n:
        ch = source[i]
        if ch in " \t\r\f\v":
            advance()
            continue
        if ch == "\n":
            start_i, start_line, start_col = i, line, col
            advance()
            emit("nl", start_i, start_line, start_col)
            continue
        if source.startswith("//", i):
            while i < n and source[i] != "\n":
                advance()
            continue
        if source.startswith("/*", i):
            depth = 1
            advance(2)
            while i < n and depth:
                if source.startswith("/*", i):
                    depth += 1
                    advance(2)
                elif source.startswith("*/", i):
                    depth -= 1
                    advance(2)
                else:
                    advance()
            continue
        if ch in ('"', "'", "`"):
            quote = ch
            start_i, start_line, start_col = i, line, col
            advance()
            while i < n:
                if quote != "`" and source[i] == "\\":
                    advance(2)
                    continue
                if source[i] == quote:
                    advance()
                    break
                advance()
            emit("str" if quote == '"' else ("rune" if quote == "'" else "raw"), start_i, start_line, start_col)
            continue
        if ch.isdigit():
            start_i, start_line, start_col = i, line, col
            advance()
            while i < n and (source[i].isalnum() or source[i] in "_.'"):
                # Do not swallow a selector dot after an integer; Odin floats
                # are the only place where a dot belongs to the number.
                if source[i] == "." and not (i + 1 < n and source[i + 1].isdigit()):
                    break
                advance()
            emit("num", start_i, start_line, start_col)
            continue
        if ch.isalpha() or ch == "_":
            start_i, start_line, start_col = i, line, col
            advance()
            while i < n and (source[i].isalnum() or source[i] == "_"):
                advance()
            value = source[start_i:i]
            emit("kw" if value in KEYWORDS else "ident", start_i, start_line, start_col)
            continue
        matched = None
        for op in MULTI_OPS:
            if source.startswith(op, i):
                matched = op
                break
        start_i, start_line, start_col = i, line, col
        if matched:
            advance(len(matched))
            emit("op", start_i, start_line, start_col)
        else:
            advance()
            emit("op", start_i, start_line, start_col)
    tokens.append(Token("eof", "", line, col, line, col))
    return tokens


# ---------------------------------------------------------------------------
# Parsed model
# ---------------------------------------------------------------------------

NODE_PACKAGE = "package"
NODE_STRUCT = "struct"
NODE_UNION = "union"
NODE_ENUM = "enum"
NODE_ENUM_CONSTANT = "enum_constant"
NODE_FIELD = "field"
NODE_FUNCTION = "function"
NODE_GLOBAL = "global"
NODE_TYPEDEF = "typedef"
NODE_TYPE_PARAMETER = "type_parameter"


@dataclass
class Decl:
    parts: Tuple[str, ...]
    kind: str
    tok: Token
    scope: Optional[Token] = None
    end: Optional[Token] = None


@dataclass
class Ref:
    context: Tuple[str, ...]
    target: Optional[Tuple[str, ...]]
    name: Optional[str]
    kind: str
    tok: Token


@dataclass
class FileInfo:
    path: str
    package: str
    package_tok: Optional[Token]
    imports: List[Tuple[Optional[str], str, Token]] = field(default_factory=list)
    decls: List[Decl] = field(default_factory=list)
    refs: List[Ref] = field(default_factory=list)
    parse_errors: List[Tuple[str, Token]] = field(default_factory=list)


def split_params(tokens: Sequence[Token]) -> List[List[Token]]:
    """Split a parameter list, keeping `a, b: int` together."""
    segments: List[List[Token]] = []
    current: List[Token] = []
    depth = 0
    for token in tokens:
        if token.value in ("(", "[", "{"):
            depth += 1
        elif token.value in (")", "]", "}"):
            depth -= 1
        if token.value == "," and depth == 0:
            segments.append(current)
            current = []
        else:
            current.append(token)
    if current:
        segments.append(current)

    merged: List[List[Token]] = []
    pending: List[Token] = []
    for segment in segments:
        if not any(token.value == ":" for token in segment):
            pending.extend(segment)
            continue
        merged.append(pending + segment)
        pending = []
    if pending:
        merged.append(pending)
    return merged


def find_body_locals(tokens: Sequence[Token]) -> Set[str]:
    names: Set[str] = set()
    for index, token in enumerate(tokens):
        if token.kind != "ident":
            continue
        if index + 1 < len(tokens) and tokens[index + 1].value in (":=", ":"):
            names.add(token.value)
        if token.value == "for" and index + 1 < len(tokens) and tokens[index + 1].kind == "ident":
            names.add(tokens[index + 1].value)
    return names


class Parser:
    def __init__(self, path: str, source: str, full: bool = True):
        self.path = path
        self.source = source
        self.tokens = lex(source)
        self.i = 0
        self.full = full
        self.package = ""
        self.package_tok: Optional[Token] = None
        self.imports: List[Tuple[Optional[str], str, Token]] = []
        self.decls: List[Decl] = []
        self.refs: List[Ref] = []
        self.errors: List[Tuple[str, Token]] = []

    # -- token helpers -----------------------------------------------------

    def peek(self, offset: int = 0) -> Token:
        index = self.i + offset
        if index >= len(self.tokens):
            return self.tokens[-1]
        return self.tokens[index]

    def advance(self) -> Token:
        token = self.peek()
        if token.kind != "eof":
            self.i += 1
        return token

    def at(self, value: str) -> bool:
        return self.peek().value == value

    def accept(self, value: str) -> Optional[Token]:
        if self.at(value):
            return self.advance()
        return None

    def expect(self, value: str) -> Optional[Token]:
        token = self.accept(value)
        if token is None:
            self.errors.append((f"expected '{value}'", self.peek()))
        return token

    def skip_to_nl(self) -> None:
        while self.peek().kind not in ("nl", "eof"):
            self.advance()
        if self.peek().kind == "nl":
            self.advance()

    def skip_balanced(self, open_value: str, close_value: str) -> List[Token]:
        start = self.i
        if not self.at(open_value):
            return []
        depth = 0
        while self.peek().kind != "eof":
            token = self.advance()
            if token.value == open_value:
                depth += 1
            elif token.value == close_value:
                depth -= 1
                if depth == 0:
                    break
        return self.tokens[start + 1:self.i - 1]

    def collect_until(self, stop_values: Set[str], stop_at_nl: bool = True) -> List[Token]:
        result: List[Token] = []
        depth = 0
        while self.peek().kind != "eof":
            token = self.peek()
            if depth == 0 and token.value in stop_values:
                break
            if depth == 0 and stop_at_nl and token.kind == "nl":
                break
            token = self.advance()
            if token.value in ("(", "[", "{"):
                depth += 1
            elif token.value in (")", "]", "}"):
                if depth == 0:
                    # Put it back logically: callers stop on the closing token.
                    self.i -= 1
                    break
                depth -= 1
            result.append(token)
        return result

    def skip_directive(self) -> None:
        if not self.accept("#"):
            return
        if self.at("+"):
            self.advance()
            self.skip_to_nl()
            return
        if self.peek().kind in ("ident", "kw"):
            name = self.advance().value
            if self.at("("):
                self.skip_balanced("(", ")")
            if name in ("assert", "type", "load", "exists", "config"):
                # The directive may continue on the same line; leave the
                # remaining expression for the caller.
                return
        self.skip_to_nl()

    def skip_attribute(self) -> None:
        if not self.accept("@"):
            return
        if self.at("("):
            self.skip_balanced("(", ")")

    # -- top level ---------------------------------------------------------

    def parse(self) -> FileInfo:
        while self.peek().kind != "eof":
            token = self.peek()
            if token.kind == "nl" or token.value == ";":
                self.advance()
                continue
            if token.value == "#":
                self.skip_directive()
                continue
            if token.value == "@":
                self.skip_attribute()
                continue
            if token.value == "package":
                self.advance()
                name = self.peek()
                if name.kind == "ident":
                    self.package = name.value
                    self.package_tok = name
                    self.advance()
                self.accept(";")
                continue
            if token.value == "import":
                self.parse_import()
                continue
            if token.value == "foreign":
                self.parse_foreign()
                continue
            if token.value == "when":
                self.parse_when()
                continue
            if token.kind == "ident":
                self.parse_declaration()
                continue
            self.advance()

        if not self.package:
            self.package = os.path.splitext(os.path.basename(self.path))[0] or "unknown"
        return FileInfo(self.path, self.package, self.package_tok, self.imports,
                        self.decls, self.refs, self.errors)

    def parse_import(self) -> None:
        start = self.advance()  # import
        alias = None
        if self.peek().kind == "ident" and self.peek(1).kind in ("str", "raw"):
            alias = self.advance().value
        path_token = self.peek()
        if path_token.kind in ("str", "raw"):
            self.advance()
            path = path_token.value
            if path.startswith(("'", '"', "`")) and path.endswith(path[0]):
                path = path[1:-1]
            self.imports.append((alias, path, path_token))
        else:
            self.skip_to_nl()
        self.accept(";")

    def parse_foreign(self) -> None:
        self.advance()  # foreign
        if self.at("import"):
            self.skip_to_nl()
            return
        if self.peek().kind == "ident":
            self.advance()
        if self.at("{"):
            # Foreign blocks contain procs and fields. Parse declarations
            # inside the block with the current package.
            self.advance()
            while self.peek().kind != "eof" and not self.at("}"):
                if self.peek().kind == "nl" or self.at(";"):
                    self.advance()
                    continue
                if self.peek().kind == "ident":
                    self.parse_declaration()
                else:
                    self.advance()
            self.accept("}")
        else:
            self.skip_to_nl()

    def parse_when(self) -> None:
        self.advance()  # when
        # Skip the condition up to the opening brace.
        while self.peek().kind != "eof" and not self.at("{"):
            if self.peek().kind == "nl":
                # A `when` without a block is not useful for symbols.
                return
            self.advance()
        if self.at("{"):
            self.advance()
            self.parse_declaration_block()
            self.accept("}")
        if self.at("else"):
            self.advance()
            if self.at("{"):
                self.advance()
                self.parse_declaration_block()
                self.accept("}")

    def parse_declaration_block(self) -> None:
        while self.peek().kind != "eof" and not self.at("}"):
            if self.peek().kind == "nl" or self.at(";"):
                self.advance()
                continue
            if self.at("#"):
                self.skip_directive()
                continue
            if self.at("@"):
                self.skip_attribute()
                continue
            if self.peek().kind == "ident":
                self.parse_declaration()
            else:
                self.advance()

    def parse_declaration(self) -> None:
        names: List[Token] = []
        if self.peek().kind != "ident":
            self.advance()
            return
        names.append(self.advance())
        while self.at(","):
            self.advance()
            if self.peek().kind == "ident":
                names.append(self.advance())
            else:
                break

        if self.accept("::"):
            for name in names:
                self.parse_value(name)
            return

        if self.accept(":"):
            # Package-level variable: name: Type = value
            for name in names:
                parts = (self.package, name.value)
                self.decls.append(Decl(parts, NODE_GLOBAL, name))
                type_tokens = self.collect_until({",", "="}, stop_at_nl=True)
                if self.full:
                    self.scan_type_tokens(type_tokens, parts)
                if self.accept("="):
                    expr = self.collect_until({",", ";"}, stop_at_nl=True)
                    if self.full:
                        self.scan_expr_tokens(expr, parts, set())
                self.accept(";")
            return

        # Not a declaration we understand; skip the rest of the line.
        self.skip_to_nl()

    # -- values ------------------------------------------------------------

    def parse_value(self, name: Token) -> None:
        parts = (self.package, name.value)
        token = self.peek()

        if token.value == "proc":
            self.parse_proc(parts, name)
            return
        if token.value == "struct":
            self.advance()
            self.skip_directive_tokens()
            self.decls.append(Decl(parts, NODE_STRUCT, name))
            if self.at("{"):
                self.parse_struct_fields(parts)
            else:
                self.skip_to_nl()
            return
        if token.value == "enum":
            self.advance()
            backing = self.collect_until({"{"}, stop_at_nl=False)
            if self.full:
                self.scan_type_tokens(backing, parts)
            self.decls.append(Decl(parts, NODE_ENUM, name))
            if self.at("{"):
                self.parse_enum_variants(parts)
            else:
                self.skip_to_nl()
            return
        if token.value == "union":
            self.advance()
            self.skip_directive_tokens()
            self.decls.append(Decl(parts, NODE_UNION, name))
            if self.at("{"):
                self.parse_union_variants(parts)
            else:
                self.skip_to_nl()
            return
        if token.value == "bit_set":
            self.advance()
            if self.at("["):
                inside = self.skip_balanced("[", "]")
                if self.full:
                    self.scan_type_tokens(inside, parts)
            self.decls.append(Decl(parts, NODE_TYPEDEF, name))
            self.skip_to_nl()
            return
        if token.value == "bit_field":
            self.advance()
            backing = self.collect_until({"{"}, stop_at_nl=False)
            if self.full:
                self.scan_type_tokens(backing, parts)
            self.decls.append(Decl(parts, NODE_STRUCT, name))
            if self.at("{"):
                self.parse_bit_field_fields(parts)
            else:
                self.skip_to_nl()
            return
        if token.value == "distinct":
            self.advance()
            type_tokens = self.collect_until({",", ";"}, stop_at_nl=True)
            if self.full:
                self.scan_type_tokens(type_tokens, parts)
            self.decls.append(Decl(parts, NODE_TYPEDEF, name))
            return
        if token.value == "#":
            self.advance()
            if self.peek().kind in ("ident", "kw"):
                directive = self.advance().value
                if directive == "type":
                    self.parse_value(name)
                    return
            self.skip_to_nl()
            return

        # Constants and any other value. Capture the expression so calls and
        # type usages inside it are visible.
        expr = self.collect_until({",", ";"}, stop_at_nl=True)
        self.decls.append(Decl(parts, NODE_GLOBAL, name))
        if self.full:
            self.scan_expr_tokens(expr, parts, set())

    def skip_directive_tokens(self) -> None:
        while self.at("#"):
            self.advance()
            if self.peek().kind in ("ident", "kw"):
                self.advance()
            if self.at("("):
                self.skip_balanced("(", ")")

    # -- aggregates --------------------------------------------------------

    def parse_struct_fields(self, owner: Tuple[str, ...]) -> None:
        self.expect("{")
        while self.peek().kind != "eof" and not self.at("}"):
            if self.peek().kind == "nl" or self.at(";") or self.at(","):
                self.advance()
                continue
            if self.at("#"):
                self.skip_directive()
                continue
            if self.at("@"):
                self.skip_attribute()
                continue
            using = bool(self.accept("using"))
            field_names: List[Token] = []
            while self.peek().kind == "ident":
                field_names.append(self.advance())
                if self.at(","):
                    self.advance()
                    continue
                break
            if not field_names:
                self.advance()
                continue
            if not self.accept(":"):
                self.skip_to_nl()
                continue
            type_tokens = self.collect_until({",", "}"}, stop_at_nl=True)
            for field_tok in field_names:
                parts = owner + (field_tok.value,)
                self.decls.append(Decl(parts, NODE_FIELD, field_tok))
                if self.full:
                    self.scan_type_tokens(type_tokens, parts)
                    if using:
                        self.add_inheritance_refs(owner, type_tokens, field_tok)
            if self.accept("="):
                expr = self.collect_until({",", "}"}, stop_at_nl=True)
                if self.full:
                    self.scan_expr_tokens(expr, owner, set())
            self.accept(",")
        self.accept("}")

    def parse_bit_field_fields(self, owner: Tuple[str, ...]) -> None:
        self.expect("{")
        while self.peek().kind != "eof" and not self.at("}"):
            if self.peek().kind == "nl" or self.at(";") or self.at(","):
                self.advance()
                continue
            if self.at("#"):
                self.skip_directive()
                continue
            field_names: List[Token] = []
            while self.peek().kind == "ident":
                field_names.append(self.advance())
                if self.at(","):
                    self.advance()
                    continue
                break
            if not field_names or not self.accept(":"):
                self.advance()
                continue
            type_tokens = self.collect_until({",", "|", "}"}, stop_at_nl=True)
            for field_tok in field_names:
                parts = owner + (field_tok.value,)
                self.decls.append(Decl(parts, NODE_FIELD, field_tok))
                if self.full:
                    self.scan_type_tokens(type_tokens, parts)
            if self.accept("|"):
                self.collect_until({",", "}"}, stop_at_nl=True)
            self.accept(",")
        self.accept("}")

    def parse_enum_variants(self, owner: Tuple[str, ...]) -> None:
        self.expect("{")
        while self.peek().kind != "eof" and not self.at("}"):
            if self.peek().kind == "nl" or self.at(";") or self.at(","):
                self.advance()
                continue
            if self.at("#"):
                self.skip_directive()
                continue
            if self.peek().kind == "ident":
                variant = self.advance()
                parts = owner + (variant.value,)
                self.decls.append(Decl(parts, NODE_ENUM_CONSTANT, variant))
                if self.accept("="):
                    expr = self.collect_until({",", "}"}, stop_at_nl=True)
                    if self.full:
                        self.scan_expr_tokens(expr, parts, set())
            else:
                self.advance()
            self.accept(",")
        self.accept("}")

    def parse_union_variants(self, owner: Tuple[str, ...]) -> None:
        self.expect("{")
        while self.peek().kind != "eof" and not self.at("}"):
            if self.peek().kind == "nl" or self.at(","):
                self.advance()
                continue
            type_tokens = self.collect_until({",", "}"}, stop_at_nl=True)
            if type_tokens and self.full:
                self.scan_type_tokens(type_tokens, owner)
            self.accept(",")
        self.accept("}")

    # -- procedures --------------------------------------------------------

    def parse_proc(self, parts: Tuple[str, ...], name: Token) -> None:
        self.advance()  # proc
        # Optional calling convention or directives before the parameter list.
        while self.peek().kind != "eof" and not self.at("(") and not self.at("{"):
            if self.peek().kind == "nl":
                break
            self.advance()
        if self.at("{"):
            # Proc group: index the group as a function, skip the list.
            self.decls.append(Decl(parts, NODE_FUNCTION, name))
            self.skip_balanced("{", "}")
            return
        if not self.at("("):
            self.decls.append(Decl(parts, NODE_FUNCTION, name))
            self.skip_to_nl()
            return

        self.advance()  # (
        param_tokens = self.collect_until({")"}, stop_at_nl=False)
        self.accept(")")
        local_names: Set[str] = set()
        for group in split_params(param_tokens):
            self.process_param_group(group, parts, local_names)

        while self.at("#"):
            self.skip_directive()
        if self.at("->"):
            self.advance()
            result_tokens = self.collect_until({"{", "where"}, stop_at_nl=True)
            if self.full:
                self.scan_type_tokens(result_tokens, parts)
        if self.at("where"):
            while self.peek().kind != "eof" and not self.at("{") and self.peek().kind != "nl":
                self.advance()

        scope_start = None
        body: List[Token] = []
        if self.at("{"):
            scope_start = self.peek()
            body = self.skip_balanced("{", "}")
        decl = Decl(parts, NODE_FUNCTION, name, scope=scope_start)
        self.decls.append(decl)
        if self.full and body:
            local_names |= find_body_locals(body)
            self.scan_expr_tokens(body, parts, local_names)

    def process_param_group(self, group: List[Token], fn_parts: Tuple[str, ...], local_names: Set[str]) -> None:
        if not group:
            return
        colon = None
        depth = 0
        for index, token in enumerate(group):
            if token.value in ("(", "[", "{"):
                depth += 1
            elif token.value in (")", "]", "}"):
                depth -= 1
            elif token.value == ":" and depth == 0:
                colon = index
                break
        if colon is None:
            return
        names = []
        for token in group[:colon]:
            if token.kind == "ident":
                names.append(token)
        type_tokens = group[colon + 1:]
        # Drop a default value from the type expression.
        for index, token in enumerate(type_tokens):
            if token.value == "=":
                type_tokens = type_tokens[:index]
                break
        for token in names:
            local_names.add(token.value)
        if self.full:
            self.scan_type_tokens(type_tokens, fn_parts)

    # -- references --------------------------------------------------------

    def scan_type_tokens(self, tokens: Sequence[Token], context: Tuple[str, ...]) -> None:
        self.scan_tokens(tokens, context, set(), default_kind="type")

    def scan_expr_tokens(self, tokens: Sequence[Token], context: Tuple[str, ...], local_names: Set[str]) -> None:
        self.scan_tokens(tokens, context, local_names, default_kind="usage")

    def scan_tokens(self, tokens: Sequence[Token], context: Tuple[str, ...],
                    local_names: Set[str], default_kind: str) -> None:
        index = 0
        brace_depth = 0
        while index < len(tokens):
            token = tokens[index]
            if token.value == "{":
                brace_depth += 1
                index += 1
                continue
            if token.value == "}":
                brace_depth = max(0, brace_depth - 1)
                index += 1
                continue
            if token.value == "#":
                index += 2
                continue
            if token.value == "@":
                if index + 1 < len(tokens) and tokens[index + 1].value == "(":
                    depth = 0
                    index += 1
                    while index < len(tokens):
                        if tokens[index].value == "(":
                            depth += 1
                        elif tokens[index].value == ")":
                            depth -= 1
                            if depth == 0:
                                index += 1
                                break
                        index += 1
                else:
                    index += 1
                continue
            if token.kind != "ident":
                index += 1
                continue
            if token.value in local_names:
                cursor = index + 1
                while (cursor + 1 < len(tokens) and tokens[cursor].value == "." and
                       tokens[cursor + 1].kind == "ident"):
                    cursor += 2
                index = cursor
                continue
            # Struct/map literal keys are field names, not references.
            if (brace_depth > 0 and index + 1 < len(tokens) and
                    tokens[index + 1].value == "=" and
                    (index == 0 or tokens[index - 1].value in ("{", ","))):
                index += 1
                continue
            chain = [token.value]
            cursor = index + 1
            while cursor + 1 < len(tokens) and tokens[cursor].value == "." and tokens[cursor + 1].kind == "ident":
                chain.append(tokens[cursor + 1].value)
                cursor += 2
            kind = "call" if cursor < len(tokens) and tokens[cursor].value == "(" else default_kind
            target = self.resolve(chain)
            if target is not None and target != context:
                self.refs.append(Ref(context, target, None, kind, token))
            index = max(index + 1, cursor)

    def add_inheritance_refs(self, context: Tuple[str, ...], tokens: Sequence[Token], tok: Token) -> None:
        for token in tokens:
            if token.kind != "ident":
                continue
            target = self.resolve([token.value])
            if target is not None:
                self.refs.append(Ref(context, target, None, "inheritance", tok))
                break

    def resolve(self, chain: Sequence[str]) -> Optional[Tuple[str, ...]]:
        if not chain:
            return None
        if len(chain) == 1 and chain[0] in BUILTIN_TYPES:
            return None
        if len(chain) == 1:
            return (self.package, chain[0])
        first, rest = chain[0], chain[1:]
        # Imported package alias: m.add -> (mathx, add)
        for alias, path, _ in self.imports:
            imported_package = import_package_name(path)
            if first == (alias or imported_package):
                return (imported_package,) + tuple(rest)
        # Type/field in the current package: Color.Red -> (main, Color, Red)
        candidate = (self.package, first) + tuple(rest)
        return candidate


def import_package_name(path: str) -> str:
    if ":" in path:
        path = path.split(":", 1)[1]
    path = path.rstrip("/\\")
    name = path.replace("\\", "/").split("/")[-1]
    return name or path


# ---------------------------------------------------------------------------
# Project symbol collection
# ---------------------------------------------------------------------------

def find_project_root(source_path: str) -> str:
    directory = os.path.dirname(os.path.abspath(source_path))
    current = directory
    for _ in range(8):
        try:
            entries = os.listdir(current)
        except OSError:
            break
        if any(name.endswith(".srctrlprj") for name in entries):
            return current
        parent = os.path.dirname(current)
        if parent == current:
            break
        current = parent
    return directory


def iter_odin_files(root: str) -> Iterable[str]:
    skipped = {".git", "build", "build-embedded", ".sourcetrail", "graphify-out"}
    for directory, dirnames, filenames in os.walk(root):
        dirnames[:] = [name for name in dirnames if name not in skipped]
        for name in filenames:
            if name.endswith(".odin"):
                yield os.path.join(directory, name)


def package_files(directory: str) -> Iterable[str]:
    """Odin packages are directories; do not descend into subpackages."""
    try:
        entries = os.listdir(directory)
    except OSError:
        return
    for name in entries:
        if name.endswith(".odin"):
            yield os.path.join(directory, name)


def file_symbol_paths(source_path: str, imports: Sequence[Tuple[Optional[str], str, Token]]) -> List[str]:
    directory = os.path.dirname(os.path.abspath(source_path))
    paths = list(package_files(directory))
    for _, path, _ in imports:
        if path.startswith(("core:", "base:", "vendor:")):
            continue
        candidate = os.path.normpath(os.path.join(directory, path))
        if os.path.isdir(candidate):
            paths.extend(package_files(candidate))
    seen = set()
    result = []
    for path in paths:
        normalized = os.path.normcase(os.path.abspath(path))
        if normalized not in seen:
            seen.add(normalized)
            result.append(path)
    return result


@dataclass
class SymbolTable:
    kinds: dict
    packages: Set[str]


def collect_symbols(paths: Iterable[str]) -> SymbolTable:
    kinds = {}
    packages = set()
    for path in paths:
        try:
            with open(path, "r", encoding="utf-8") as handle:
                source = handle.read()
            info = Parser(path, source, full=False).parse()
        except (OSError, UnicodeError):
            continue
        packages.add(info.package)
        for decl in info.decls:
            kinds[decl.parts] = decl.kind
    return SymbolTable(kinds, packages)


# ---------------------------------------------------------------------------
# Sourcetrail writer
# ---------------------------------------------------------------------------

KIND_TO_SYMBOL = {}
REF_TO_EDGE = {}


def init_db_constants(srctrl) -> None:
    if KIND_TO_SYMBOL:
        return
    KIND_TO_SYMBOL.update({
        NODE_PACKAGE: srctrl.SYMBOL_PACKAGE,
        NODE_STRUCT: srctrl.SYMBOL_STRUCT,
        NODE_UNION: srctrl.SYMBOL_UNION,
        NODE_ENUM: srctrl.SYMBOL_ENUM,
        NODE_ENUM_CONSTANT: srctrl.SYMBOL_ENUM_CONSTANT,
        NODE_FIELD: srctrl.SYMBOL_FIELD,
        NODE_FUNCTION: srctrl.SYMBOL_FUNCTION,
        NODE_GLOBAL: srctrl.SYMBOL_GLOBAL_VARIABLE,
        NODE_TYPEDEF: srctrl.SYMBOL_TYPEDEF,
        NODE_TYPE_PARAMETER: srctrl.SYMBOL_TYPE_PARAMETER,
    })
    REF_TO_EDGE.update({
        "call": srctrl.REFERENCE_CALL,
        "type": srctrl.REFERENCE_TYPE_USAGE,
        "usage": srctrl.REFERENCE_USAGE,
        "import": srctrl.REFERENCE_IMPORT,
        "inheritance": srctrl.REFERENCE_INHERITANCE,
    })


def name_json(parts: Sequence[str]) -> str:
    return json.dumps({
        "name_delimiter": ".",
        "name_elements": [
            {"prefix": "", "name": part, "postfix": ""} for part in parts
        ],
    })


class Writer:
    def __init__(self, srctrl, file_id: int, table: SymbolTable):
        self.srctrl = srctrl
        self.file_id = file_id
        self.table = table
        self.ids = {}
        self.written_refs = set()

    def symbol_id(self, parts: Sequence[str], kind: Optional[str] = None,
                  tok: Optional[Token] = None, scope: Optional[Token] = None) -> int:
        key = tuple(parts)
        if key in self.ids:
            return self.ids[key]
        symbol_id = self.srctrl.recordSymbol(name_json(key))
        if not symbol_id:
            return 0
        self.ids[key] = symbol_id
        self.srctrl.recordSymbolDefinitionKind(symbol_id, self.srctrl.DEFINITION_EXPLICIT)
        symbol_kind = kind or self.table.kinds.get(key, NODE_GLOBAL)
        self.srctrl.recordSymbolKind(symbol_id, KIND_TO_SYMBOL.get(symbol_kind, self.srctrl.SYMBOL_GLOBAL_VARIABLE))
        if tok is not None:
            self.srctrl.recordSymbolLocation(symbol_id, self.file_id, tok.line, tok.col, tok.end_line, tok.end_col)
            if scope is not None:
                self.srctrl.recordSymbolScopeLocation(
                    symbol_id, self.file_id, tok.line, tok.col, scope.end_line, scope.end_col)
        return symbol_id

    def reference(self, context_id: int, target_parts: Sequence[str], kind: str, tok: Token) -> None:
        key = (context_id, tuple(target_parts), kind, tok.line, tok.col)
        if key in self.written_refs:
            return
        self.written_refs.add(key)
        target_key = tuple(target_parts)
        target_kind = self.table.kinds.get(target_key)
        if target_kind is None:
            target_kind = NODE_FUNCTION if kind == "call" else (
                NODE_TYPEDEF if kind == "type" else NODE_GLOBAL)
        target_id = self.symbol_id(target_key, target_kind)
        if not target_id:
            return
        edge = REF_TO_EDGE.get(kind, self.srctrl.REFERENCE_USAGE)
        reference_id = self.srctrl.recordReference(context_id, target_id, edge)
        if reference_id:
            self.srctrl.recordReferenceLocation(
                reference_id, self.file_id, tok.line, tok.col, tok.end_line, tok.end_col)

    def error(self, message: str, tok: Token, fatal: bool = False) -> None:
        self.srctrl.recordError(
            message, fatal, self.file_id, tok.line, tok.col, tok.end_line, tok.end_col)


def index_file(source_path: str, database_path: str, verbose: bool) -> int:
    trace("start")
    srctrl = load_srctrl()
    init_db_constants(srctrl)
    trace("open")
    if not srctrl.open(os.path.abspath(database_path)):
        print("ERROR: cannot open database: " + str(srctrl.getLastError()))
        return 1
    trace("opened")

    try:
        with open(source_path, "r", encoding="utf-8") as handle:
            source = handle.read()
        info = Parser(source_path, source, full=True).parse()
        trace("parsed")
        table = collect_symbols(file_symbol_paths(source_path, info.imports))
        table.kinds.setdefault((info.package,), NODE_PACKAGE)
        trace("collected")

        file_id = srctrl.recordFile(os.path.abspath(source_path).replace("\\", "/"))
        if not file_id:
            print("ERROR: cannot record file: " + str(srctrl.getLastError()))
            return 1
        srctrl.recordFileLanguage(file_id, "odin")
        writer = Writer(srctrl, file_id, table)

        srctrl.beginTransaction()
        package_tok = info.package_tok
        if package_tok is None:
            package_tok = info.imports[0][2] if info.imports else Token("", "", 1, 1, 1, 1)
        package_id = writer.symbol_id((info.package,), NODE_PACKAGE, package_tok)

        for alias, path, tok in info.imports:
            package_name = import_package_name(path)
            target = writer.symbol_id((package_name,), NODE_PACKAGE)
            if package_id and target:
                writer.reference(package_id, (package_name,), "import", tok)

        for decl in info.decls:
            writer.symbol_id(decl.parts, decl.kind, decl.tok, decl.scope)

        for ref in info.refs:
            context_id = writer.symbol_id(ref.context)
            if not context_id:
                continue
            if ref.target is not None:
                writer.reference(context_id, ref.target, ref.kind, ref.tok)
            elif ref.kind == "call":
                srctrl.recordReferenceToUnsolvedSymhol(
                    context_id, srctrl.REFERENCE_CALL, file_id,
                    ref.tok.line, ref.tok.col, ref.tok.end_line, ref.tok.end_col)

        for message, token in info.parse_errors[:20]:
            writer.error(message, token, False)
        trace("writing done")
        srctrl.commitTransaction()
        trace("committed")
        if verbose:
            print("INFO: indexed %s (%d declarations, %d references)" %
                  (source_path, len(info.decls), len(info.refs)))
        return 0
    finally:
        trace("closing")
        srctrl.close()
        trace("closed")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def dump_file(source_path: str) -> int:
    with open(source_path, "r", encoding="utf-8") as handle:
        source = handle.read()
    info = Parser(source_path, source, full=True).parse()
    print("package:", info.package)
    for alias, path, tok in info.imports:
        print("import:", alias, path, tok.line, tok.col)
    for decl in info.decls:
        print("decl:", ".".join(decl.parts), decl.kind, decl.tok.line, decl.tok.col)
    for ref in info.refs:
        print("ref:", ".".join(ref.context), "->",
              ".".join(ref.target) if ref.target else "?", ref.kind, ref.tok.line, ref.tok.col)
    for message, token in info.parse_errors:
        print("error:", message, token.line, token.col)
    return 0


def main(argv: Optional[Sequence[str]] = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    if not argv:
        print("usage: run.py {index|dump} ...")
        return 2
    command = argv.pop(0)
    if command == "dump":
        parser = argparse.ArgumentParser(prog="run.py dump")
        parser.add_argument("source_file_path")
        args = parser.parse_args(argv)
        return dump_file(args.source_file_path)

    parser = argparse.ArgumentParser(prog="run.py index")
    parser.add_argument("--source-file-path", required=True)
    parser.add_argument("--database-file-path", required=True)
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)
    return index_file(args.source_file_path, args.database_file_path, args.verbose)


if __name__ == "__main__":
    sys.exit(main())
