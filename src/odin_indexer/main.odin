package main

import "core:c"
import "core:fmt"
import "core:os"
import "core:path/filepath"
import "core:strings"

foreign import writer "sourcetrail_odin_writer.lib"

@(default_calling_convention="c")
foreign writer {
	sodb_open                  :: proc(path: cstring) -> rawptr ---
	sodb_close                 :: proc(w: rawptr) ---
	sodb_begin                 :: proc(w: rawptr) -> c.int ---
	sodb_commit                :: proc(w: rawptr) -> c.int ---
	sodb_symbol                :: proc(w: rawptr, full_name: cstring, delimiter: cstring, kind: c.int) -> c.int ---
	sodb_symbol_location       :: proc(w: rawptr, id: c.int, file: c.int, sl: c.int, sc: c.int, el: c.int, ec: c.int) -> c.int ---
	sodb_symbol_scope_location :: proc(w: rawptr, id: c.int, file: c.int, sl: c.int, sc: c.int, el: c.int, ec: c.int) -> c.int ---
	sodb_reference             :: proc(w: rawptr, source: c.int, target: c.int, kind: c.int) -> c.int ---
	sodb_reference_location    :: proc(w: rawptr, ref: c.int, file: c.int, sl: c.int, sc: c.int, el: c.int, ec: c.int) -> c.int ---
	sodb_file                  :: proc(w: rawptr, path: cstring) -> c.int ---
	sodb_file_language         :: proc(w: rawptr, file: c.int, language: cstring) -> c.int ---
	sodb_error                 :: proc(w: rawptr, message: cstring, fatal: c.int, file: c.int, sl: c.int, sc: c.int, el: c.int, ec: c.int) -> c.int ---
	sodb_last_error            :: proc(w: rawptr) -> cstring ---
}

Token_Kind :: enum {
	Ident,
	Keyword,
	Number,
	String,
	Rune,
	Raw,
	Op,
	Newline,
	Eof,
}

Token :: struct {
	kind:     Token_Kind,
	text:     string,
	line:     int,
	col:      int,
	end_line: int,
	end_col:  int,
}

Symbol_Kind :: enum int {
	TYPE            = 0,
	BUILTIN_TYPE    = 1,
	MODULE          = 2,
	NAMESPACE       = 3,
	PACKAGE         = 4,
	STRUCT          = 5,
	CLASS           = 6,
	INTERFACE       = 7,
	ANNOTATION      = 8,
	GLOBAL_VARIABLE = 9,
	FIELD           = 10,
	FUNCTION        = 11,
	METHOD          = 12,
	ENUM            = 13,
	ENUM_CONSTANT   = 14,
	TYPEDEF         = 15,
	TYPE_PARAMETER  = 16,
	MACRO           = 17,
	UNION           = 18,
}

Ref_Kind :: enum int {
	TYPE_USAGE  = 0,
	USAGE       = 1,
	CALL        = 2,
	INHERITANCE = 3,
	IMPORT      = 8,
}

Decl :: struct {
	name:  string,
	kind:  Symbol_Kind,
	tok:   Token,
	scope: Token,
	has_scope: bool,
}

Ref :: struct {
	ctx: string,
	target:  string,
	kind:    Ref_Kind,
	tok:     Token,
}

Import :: struct {
	alias:   string,
	path:    string,
	pkg: string,
	tok:     Token,
}

File_Info :: struct {
	pkg:         string,
	pkg_tok:     Token,
	has_package: bool,
	imports:    [dynamic]Import,
	decls:      [dynamic]Decl,
	refs:       [dynamic]Ref,
}

Parser :: struct {
	tokens: []Token,
	index:  int,
	full:   bool,
	file:   File_Info,
}

MULTI_OPS := []string{"::", ":=", "->", "..<", "..=", "...", "==", "!=", "<=", ">=", "&&", "||", "<<", ">>", "&~", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="}

is_keyword :: proc(s: string) -> bool {
	switch s {
	case "package", "import", "foreign", "proc", "struct", "union", "enum",
		"bit_set", "bit_field", "distinct", "map", "matrix", "using", "when",
		"where", "if", "else", "for", "in", "switch", "case", "defer", "return",
		"break", "continue", "fallthrough", "or_return", "or_else", "or_break",
		"or_continue", "nil", "true", "false", "cast", "transmute", "type_of",
		"type_info_of", "size_of", "align_of", "offset_of", "context":
		return true
	}
	return false
}

is_builtin_type :: proc(s: string) -> bool {
	switch s {
	case "int", "uint", "uintptr", "typeid", "rawptr", "string", "cstring", "bool",
		"b8", "b16", "b32", "b64", "rune", "any", "byte", "u8", "u16", "u32",
		"u64", "u128", "i8", "i16", "i32", "i64", "i128", "f16", "f32", "f64",
		"f128", "complex32", "complex64", "complex128", "quaternion64",
		"quaternion128", "quaternion256", "intle", "intbe", "uintle", "uintbe":
		return true
	}
	return false
}

is_ident_start :: proc(b: byte) -> bool {
	return (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || b == '_' || b >= 0x80
}

is_ident_char :: proc(b: byte) -> bool {
	return is_ident_start(b) || (b >= '0' && b <= '9')
}

lex :: proc(source: string) -> []Token {
	tokens := make([dynamic]Token, 0, len(source) / 4 + 16)
	i := 0
	line := 1
	col := 1
	n := len(source)

	advance :: proc(source: string, i: ^int, line: ^int, col: ^int, count: int = 1) {
		for _ in 0 ..< count {
			if i^ >= len(source) {
				return
			}
			if source[i^] == '\n' {
				line^ += 1
				col^ = 1
			} else {
				col^ += 1
			}
			i^ += 1
		}
	}

	emit :: proc(tokens: ^[dynamic]Token, source: string, kind: Token_Kind, start_i: int, start_line: int, start_col: int, i: int, line: int, col: int) {
		append(tokens, Token{kind = kind, text = source[start_i:i], line = start_line, col = start_col, end_line = line, end_col = col})
	}

	for i < n {
		ch := source[i]
		if ch == ' ' || ch == '\t' || ch == '\r' || ch == '\f' || ch == '\v' {
			advance(source, &i, &line, &col)
			continue
		}
		if ch == '\n' {
			start_i, start_line, start_col := i, line, col
			advance(source, &i, &line, &col)
			emit(&tokens, source, .Newline, start_i, start_line, start_col, i, line, col)
			continue
		}
		if ch == '/' && i + 1 < n && source[i + 1] == '/' {
			for i < n && source[i] != '\n' {
				advance(source, &i, &line, &col)
			}
			continue
		}
		if ch == '/' && i + 1 < n && source[i + 1] == '*' {
			depth := 1
			advance(source, &i, &line, &col, 2)
			for i < n && depth > 0 {
				if i + 1 < n && source[i] == '/' && source[i + 1] == '*' {
					depth += 1
					advance(source, &i, &line, &col, 2)
				} else if i + 1 < n && source[i] == '*' && source[i + 1] == '/' {
					depth -= 1
					advance(source, &i, &line, &col, 2)
				} else {
					advance(source, &i, &line, &col)
				}
			}
			continue
		}
		if ch == '"' || ch == '\'' || ch == '`' {
			quote := ch
			start_i, start_line, start_col := i, line, col
			advance(source, &i, &line, &col)
			for i < n {
				if quote != '`' && source[i] == '\\' {
					advance(source, &i, &line, &col, 2)
					continue
				}
				if source[i] == quote {
					advance(source, &i, &line, &col)
					break
				}
				advance(source, &i, &line, &col)
			}
			kind := Token_Kind.String
			if quote == '\'' {
				kind = .Rune
			} else if quote == '`' {
				kind = .Raw
			}
			emit(&tokens, source, kind, start_i, start_line, start_col, i, line, col)
			continue
		}
		if ch >= '0' && ch <= '9' {
			start_i, start_line, start_col := i, line, col
			advance(source, &i, &line, &col)
			for i < n {
				b := source[i]
				if (b >= 'a' && b <= 'z') || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9') || b == '_' || b == '\'' {
					advance(source, &i, &line, &col)
				} else if b == '.' && i + 1 < n && source[i + 1] >= '0' && source[i + 1] <= '9' {
					advance(source, &i, &line, &col)
				} else {
					break
				}
			}
			emit(&tokens, source, .Number, start_i, start_line, start_col, i, line, col)
			continue
		}
		if is_ident_start(ch) {
			start_i, start_line, start_col := i, line, col
			advance(source, &i, &line, &col)
			for i < n && is_ident_char(source[i]) {
				advance(source, &i, &line, &col)
			}
			text := source[start_i:i]
			kind := Token_Kind.Ident
			if is_keyword(text) {
				kind = .Keyword
			}
			emit(&tokens, source, kind, start_i, start_line, start_col, i, line, col)
			continue
		}
		matched := ""
		for op in MULTI_OPS {
			if strings.has_prefix(source[i:], op) {
				matched = op
				break
			}
		}
		start_i, start_line, start_col := i, line, col
		if matched != "" {
			advance(source, &i, &line, &col, len(matched))
		} else {
			advance(source, &i, &line, &col)
		}
		emit(&tokens, source, .Op, start_i, start_line, start_col, i, line, col)
	}
	append(&tokens, Token{kind = .Eof, text = "", line = line, col = col, end_line = line, end_col = col})
	return tokens[:]
}

peek :: proc(p: ^Parser, offset: int = 0) -> Token {
	index := p.index + offset
	if index < 0 || index >= len(p.tokens) {
		return p.tokens[len(p.tokens) - 1]
	}
	return p.tokens[index]
}

advance :: proc(p: ^Parser) -> Token {
	token := peek(p)
	if token.kind != .Eof {
		p.index += 1
	}
	return token
}

at :: proc(p: ^Parser, text: string) -> bool {
	return peek(p).text == text
}

accept :: proc(p: ^Parser, text: string) -> bool {
	if at(p, text) {
		advance(p)
		return true
	}
	return false
}

skip_to_nl :: proc(p: ^Parser) {
	for peek(p).kind != .Newline && peek(p).kind != .Eof {
		advance(p)
	}
	if peek(p).kind == .Newline {
		advance(p)
	}
}

skip_balanced :: proc(p: ^Parser, open: string, close: string) -> []Token {
	inner := make([dynamic]Token, 0, 16)
	if !at(p, open) {
		return inner[:]
	}
	depth := 0
	for peek(p).kind != .Eof {
		token := advance(p)
		if token.text == open {
			depth += 1
			if depth == 1 {
				continue
			}
		} else if token.text == close {
			depth -= 1
			if depth == 0 {
				break
			}
		}
		if depth > 0 {
			append(&inner, token)
		}
	}
	return inner[:]
}

skip_directive :: proc(p: ^Parser) {
	if !accept(p, "#") {
		return
	}
	if at(p, "+") {
		advance(p)
		skip_to_nl(p)
		return
	}
	name := ""
	if peek(p).kind == .Ident || peek(p).kind == .Keyword {
		name = advance(p).text
	}
	if at(p, "(") {
		skip_balanced(p, "(", ")")
	}
	if name == "assert" || name == "type" || name == "load" || name == "exists" || name == "config" {
		return
	}
	skip_to_nl(p)
}

skip_attribute :: proc(p: ^Parser) {
	if !accept(p, "@") {
		return
	}
	if at(p, "(") {
		skip_balanced(p, "(", ")")
	}
}

parse_file :: proc(path: string, source: string, full: bool) -> File_Info {
	p := Parser{tokens = lex(source), index = 0, full = full}
	p.file.imports = make([dynamic]Import)
	p.file.decls = make([dynamic]Decl)
	p.file.refs = make([dynamic]Ref)

	for peek(&p).kind != .Eof {
		token := peek(&p)
		if token.kind == .Newline || token.text == ";" {
			advance(&p)
			continue
		}
		if token.text == "#" {
			skip_directive(&p)
			continue
		}
		if token.text == "@" {
			skip_attribute(&p)
			continue
		}
		if token.text == "package" {
			advance(&p)
			name := peek(&p)
			if name.kind == .Ident {
				p.file.pkg = name.text
				p.file.pkg_tok = name
				p.file.has_package = true
				advance(&p)
			}
			accept(&p, ";")
			continue
		}
		if token.text == "import" {
			parse_import(&p)
			continue
		}
		if token.text == "foreign" {
			parse_foreign(&p)
			continue
		}
		if token.text == "when" {
			parse_when(&p)
			continue
		}
		if token.kind == .Ident {
			parse_declaration(&p)
			continue
		}
		advance(&p)
	}

	if !p.file.has_package {
		base := filepath.base(path)
		for strings.has_suffix(base, ".odin") {
			base = base[:len(base) - 5]
		}
		p.file.pkg = base
	}
	return p.file
}

parse_import :: proc(p: ^Parser) {
	advance(p) // import
	alias := ""
	if peek(p).kind == .Ident && (peek(p, 1).kind == .String || peek(p, 1).kind == .Raw) {
		alias = advance(p).text
	}
	path_token := peek(p)
	if path_token.kind == .String || path_token.kind == .Raw {
		advance(p)
		path := path_token.text
		if len(path) >= 2 {
			path = path[1:len(path) - 1]
		}
		append(&p.file.imports, Import{alias = alias, path = path, pkg = import_package_name(path), tok = path_token})
	} else {
		skip_to_nl(p)
	}
	accept(p, ";")
}

parse_foreign :: proc(p: ^Parser) {
	advance(p) // foreign
	if at(p, "import") {
		skip_to_nl(p)
		return
	}
	if peek(p).kind == .Ident {
		advance(p)
	}
	if at(p, "{") {
		advance(p)
		for peek(p).kind != .Eof && !at(p, "}") {
			if peek(p).kind == .Newline || at(p, ";") {
				advance(p)
				continue
			}
			if peek(p).kind == .Ident {
				parse_declaration(p)
			} else {
				advance(p)
			}
		}
		accept(p, "}")
	} else {
		skip_to_nl(p)
	}
}

parse_when :: proc(p: ^Parser) {
	advance(p) // when
	for peek(p).kind != .Eof && !at(p, "{") {
		if peek(p).kind == .Newline {
			return
		}
		advance(p)
	}
	if at(p, "{") {
		advance(p)
		parse_declaration_block(p)
		accept(p, "}")
	}
	if at(p, "else") {
		advance(p)
		if at(p, "{") {
			advance(p)
			parse_declaration_block(p)
			accept(p, "}")
		}
	}
}

parse_declaration_block :: proc(p: ^Parser) {
	for peek(p).kind != .Eof && !at(p, "}") {
		if peek(p).kind == .Newline || at(p, ";") {
			advance(p)
			continue
		}
		if at(p, "#") {
			skip_directive(p)
			continue
		}
		if at(p, "@") {
			skip_attribute(p)
			continue
		}
		if peek(p).kind == .Ident {
			parse_declaration(p)
		} else {
			advance(p)
		}
	}
}

parse_declaration :: proc(p: ^Parser) {
	if peek(p).kind != .Ident {
		advance(p)
		return
	}
	names := make([dynamic]Token, 0, 4)
	append(&names, advance(p))
	for at(p, ",") {
		advance(p)
		if peek(p).kind == .Ident {
			append(&names, advance(p))
		} else {
			break
		}
	}

	if accept(p, "::") {
		for name in names {
			parse_value(p, name)
		}
		return
	}
	if accept(p, ":") {
		for name in names {
			full := make_full_name(p.file.pkg, name.text)
			append(&p.file.decls, Decl{name = full, kind = .GLOBAL_VARIABLE, tok = name})
			type_tokens := collect_until(p, "=", "", true)
			if p.full {
				scan_type_tokens(p, type_tokens, full)
			}
			if accept(p, "=") {
				expr := collect_until(p, "=", ";", true)
				if p.full {
					scan_expr_tokens(p, expr, full, make(map[string]bool))
				}
			}
			accept(p, ";")
		}
		return
	}
	skip_to_nl(p)
}

parse_value :: proc(p: ^Parser, name: Token) {
	full := make_full_name(p.file.pkg, name.text)
	token := peek(p)

	if token.text == "proc" {
		parse_proc(p, full, name)
		return
	}
	if token.text == "struct" {
		advance(p)
		skip_directive_tokens(p)
		append(&p.file.decls, Decl{name = full, kind = .STRUCT, tok = name})
		if at(p, "{") {
			parse_struct_fields(p, full)
		} else {
			skip_to_nl(p)
		}
		return
	}
	if token.text == "enum" {
		advance(p)
		backing := collect_until(p, "{", "", false)
		if p.full {
			scan_type_tokens(p, backing, full)
		}
		append(&p.file.decls, Decl{name = full, kind = .ENUM, tok = name})
		if at(p, "{") {
			parse_enum_variants(p, full)
		} else {
			skip_to_nl(p)
		}
		return
	}
	if token.text == "union" {
		advance(p)
		skip_directive_tokens(p)
		append(&p.file.decls, Decl{name = full, kind = .UNION, tok = name})
		if at(p, "{") {
			parse_union_variants(p, full)
		} else {
			skip_to_nl(p)
		}
		return
	}
	if token.text == "bit_set" {
		advance(p)
		if at(p, "[") {
			inside := skip_balanced(p, "[", "]")
			if p.full {
				scan_type_tokens(p, inside, full)
			}
		}
		append(&p.file.decls, Decl{name = full, kind = .TYPEDEF, tok = name})
		skip_to_nl(p)
		return
	}
	if token.text == "bit_field" {
		advance(p)
		backing := collect_until(p, "{", "", false)
		if p.full {
			scan_type_tokens(p, backing, full)
		}
		append(&p.file.decls, Decl{name = full, kind = .STRUCT, tok = name})
		if at(p, "{") {
			parse_bit_field_fields(p, full)
		} else {
			skip_to_nl(p)
		}
		return
	}
	if token.text == "distinct" {
		advance(p)
		type_tokens := collect_until(p, "=", ";", true)
		if p.full {
			scan_type_tokens(p, type_tokens, full)
		}
		append(&p.file.decls, Decl{name = full, kind = .TYPEDEF, tok = name})
		return
	}
	if token.text == "#" {
		advance(p)
		if peek(p).kind == .Ident || peek(p).kind == .Keyword {
			if advance(p).text == "type" {
				append(&p.file.decls, Decl{name = full, kind = .TYPEDEF, tok = name})
				skip_to_nl(p)
				return
			}
		}
		skip_to_nl(p)
		return
	}

	expr := collect_until(p, "=", ";", true)
	append(&p.file.decls, Decl{name = full, kind = .GLOBAL_VARIABLE, tok = name})
	if p.full {
		scan_expr_tokens(p, expr, full, make(map[string]bool))
	}
}

skip_directive_tokens :: proc(p: ^Parser) {
	for at(p, "#") {
		advance(p)
		if peek(p).kind == .Ident || peek(p).kind == .Keyword {
			advance(p)
		}
		if at(p, "(") {
			skip_balanced(p, "(", ")")
		}
	}
}

parse_struct_fields :: proc(p: ^Parser, owner: string) {
	accept(p, "{")
	for peek(p).kind != .Eof && !at(p, "}") {
		if peek(p).kind == .Newline || at(p, ";") || at(p, ",") {
			advance(p)
			continue
		}
		if at(p, "#") {
			skip_directive(p)
			continue
		}
		if at(p, "@") {
			skip_attribute(p)
			continue
		}
		is_using := accept(p, "using")
		field_names := make([dynamic]Token, 0, 2)
		for peek(p).kind == .Ident {
			append(&field_names, advance(p))
			if at(p, ",") {
				advance(p)
				continue
			}
			break
		}
		if len(field_names) == 0 {
			advance(p)
			continue
		}
		if !accept(p, ":") {
			skip_to_nl(p)
			continue
		}
		type_tokens := collect_until(p, ",", "}", true)
		for field_tok in field_names {
			full := make_full_name(owner, field_tok.text)
			append(&p.file.decls, Decl{name = full, kind = .FIELD, tok = field_tok})
			if p.full {
				scan_type_tokens(p, type_tokens, full)
				if is_using {
					add_inheritance_refs(p, owner, type_tokens, field_tok)
				}
			}
		}
		if accept(p, "=") {
			expr := collect_until(p, ",", "}", true)
			if p.full {
				scan_expr_tokens(p, expr, owner, make(map[string]bool))
			}
		}
		accept(p, ",")
	}
	accept(p, "}")
}

parse_bit_field_fields :: proc(p: ^Parser, owner: string) {
	accept(p, "{")
	for peek(p).kind != .Eof && !at(p, "}") {
		if peek(p).kind == .Newline || at(p, ";") || at(p, ",") {
			advance(p)
			continue
		}
		if at(p, "#") {
			skip_directive(p)
			continue
		}
		field_names := make([dynamic]Token, 0, 2)
		for peek(p).kind == .Ident {
			append(&field_names, advance(p))
			if at(p, ",") {
				advance(p)
				continue
			}
			break
		}
		if len(field_names) == 0 || !accept(p, ":") {
			advance(p)
			continue
		}
		type_tokens := collect_until(p, ",", "}", true)
		for field_tok in field_names {
			full := make_full_name(owner, field_tok.text)
			append(&p.file.decls, Decl{name = full, kind = .FIELD, tok = field_tok})
			if p.full {
				scan_type_tokens(p, type_tokens, full)
			}
		}
		if accept(p, "|") {
			collect_until(p, ",", "}", true)
		}
		accept(p, ",")
	}
	accept(p, "}")
}

parse_enum_variants :: proc(p: ^Parser, owner: string) {
	accept(p, "{")
	for peek(p).kind != .Eof && !at(p, "}") {
		if peek(p).kind == .Newline || at(p, ";") || at(p, ",") {
			advance(p)
			continue
		}
		if at(p, "#") {
			skip_directive(p)
			continue
		}
		if peek(p).kind == .Ident {
			variant := advance(p)
			full := make_full_name(owner, variant.text)
			append(&p.file.decls, Decl{name = full, kind = .ENUM_CONSTANT, tok = variant})
			if accept(p, "=") {
				expr := collect_until(p, ",", "}", true)
				if p.full {
					scan_expr_tokens(p, expr, full, make(map[string]bool))
				}
			}
		} else {
			advance(p)
		}
		accept(p, ",")
	}
	accept(p, "}")
}

parse_union_variants :: proc(p: ^Parser, owner: string) {
	accept(p, "{")
	for peek(p).kind != .Eof && !at(p, "}") {
		if peek(p).kind == .Newline || at(p, ",") {
			advance(p)
			continue
		}
		type_tokens := collect_until(p, ",", "}", true)
		if len(type_tokens) > 0 && p.full {
			scan_type_tokens(p, type_tokens, owner)
		}
		accept(p, ",")
	}
	accept(p, "}")
}

parse_proc :: proc(p: ^Parser, full: string, name: Token) {
	advance(p) // proc
	for peek(p).kind != .Eof && !at(p, "(") && !at(p, "{") {
		if peek(p).kind == .Newline {
			break
		}
		advance(p)
	}
	if at(p, "{") {
		append(&p.file.decls, Decl{name = full, kind = .FUNCTION, tok = name})
		skip_balanced(p, "{", "}")
		return
	}
	if !at(p, "(") {
		append(&p.file.decls, Decl{name = full, kind = .FUNCTION, tok = name})
		skip_to_nl(p)
		return
	}

	param_tokens := skip_balanced(p, "(", ")")
	local_names := make(map[string]bool)
	groups := split_params(param_tokens)
	for group in groups {
		process_param_group(p, group, full, &local_names)
	}

	for at(p, "#") {
		skip_directive(p)
	}
	if accept(p, "->") {
		result_tokens := collect_until(p, "{", "where", true)
		if p.full {
			scan_type_tokens(p, result_tokens, full)
		}
	}
	if at(p, "where") {
		for peek(p).kind != .Eof && !at(p, "{") && peek(p).kind != .Newline {
			advance(p)
		}
	}

	scope_tok: Token
	has_scope := false
	body: []Token
	if at(p, "{") {
		scope_tok = peek(p)
		has_scope = true
		body = skip_balanced(p, "{", "}")
	}
	append(&p.file.decls, Decl{name = full, kind = .FUNCTION, tok = name, scope = scope_tok, has_scope = has_scope})
	if p.full && len(body) > 0 {
		add_body_locals(&local_names, body)
		scan_expr_tokens(p, body, full, local_names)
	}
}

split_params :: proc(tokens: []Token) -> [] []Token {
	segments := make([dynamic] []Token, 0, 4)
	current := make([dynamic]Token, 0, 8)
	depth := 0
	for token in tokens {
		if token.text == "(" || token.text == "[" || token.text == "{" {
			depth += 1
		} else if token.text == ")" || token.text == "]" || token.text == "}" {
			depth -= 1
		}
		if token.text == "," && depth == 0 {
			append(&segments, current[:])
			current = make([dynamic]Token, 0, 8)
		} else {
			append(&current, token)
		}
	}
	if len(current) > 0 {
		append(&segments, current[:])
	}

	merged := make([dynamic] []Token, 0, len(segments))
	pending := make([dynamic]Token, 0, 8)
	for segment in segments {
		has_colon := false
		for token in segment {
			if token.text == ":" {
				has_colon = true
				break
			}
		}
		if !has_colon {
			for token in segment {
				append(&pending, token)
			}
			continue
		}
		combined := make([dynamic]Token, 0, len(pending) + len(segment))
		for token in pending {
			append(&combined, token)
		}
		for token in segment {
			append(&combined, token)
		}
		append(&merged, combined[:])
		pending = make([dynamic]Token, 0, 8)
	}
	if len(pending) > 0 {
		append(&merged, pending[:])
	}
	return merged[:]
}

process_param_group :: proc(p: ^Parser, group: []Token, full: string, local_names: ^map[string]bool) {
	if len(group) == 0 {
		return
	}
	colon := -1
	depth := 0
	for token, index in group {
		if token.text == "(" || token.text == "[" || token.text == "{" {
			depth += 1
		} else if token.text == ")" || token.text == "]" || token.text == "}" {
			depth -= 1
		} else if token.text == ":" && depth == 0 {
			colon = index
			break
		}
	}
	if colon < 0 {
		return
	}
	for token in group[:colon] {
		if token.kind == .Ident {
			local_names^[token.text] = true
		}
	}
	type_tokens := group[colon + 1:]
	for token, index in type_tokens {
		if token.text == "=" {
			type_tokens = type_tokens[:index]
			break
		}
	}
	if p.full {
		scan_type_tokens(p, type_tokens, full)
	}
}

scan_type_tokens :: proc(p: ^Parser, tokens: []Token, context_name: string) {
	scan_tokens(p, tokens, context_name, make(map[string]bool), .TYPE_USAGE)
}

scan_expr_tokens :: proc(p: ^Parser, tokens: []Token, context_name: string, local_names: map[string]bool) {
	scan_tokens(p, tokens, context_name, local_names, .USAGE)
}

scan_tokens :: proc(p: ^Parser, tokens: []Token, context_name: string, local_names: map[string]bool, default_kind: Ref_Kind) {
	index := 0
	brace_depth := 0
	for index < len(tokens) {
		token := tokens[index]
		if token.text == "{" {
			brace_depth += 1
			index += 1
			continue
		}
		if token.text == "}" {
			if brace_depth > 0 {
				brace_depth -= 1
			}
			index += 1
			continue
		}
		if token.text == "#" {
			index += 2
			continue
		}
		if token.text == "@" {
			if index + 1 < len(tokens) && tokens[index + 1].text == "(" {
				depth := 0
				index += 1
				for index < len(tokens) {
					if tokens[index].text == "(" {
						depth += 1
					} else if tokens[index].text == ")" {
						depth -= 1
						if depth == 0 {
							index += 1
							break
						}
					}
					index += 1
				}
			} else {
				index += 1
			}
			continue
		}
		if token.kind != .Ident {
			index += 1
			continue
		}
		if local_names[token.text] {
			cursor := index + 1
			for cursor + 1 < len(tokens) && tokens[cursor].text == "." && tokens[cursor + 1].kind == .Ident {
				cursor += 2
			}
			index = cursor
			continue
		}
		if brace_depth > 0 && index + 1 < len(tokens) && tokens[index + 1].text == "=" &&
			(index == 0 || tokens[index - 1].text == "{" || tokens[index - 1].text == ",") {
			index += 1
			continue
		}

		chain := make([dynamic]string, 0, 4)
		append(&chain, token.text)
		cursor := index + 1
		for cursor + 1 < len(tokens) && tokens[cursor].text == "." && tokens[cursor + 1].kind == .Ident {
			append(&chain, tokens[cursor + 1].text)
			cursor += 2
		}
		kind := default_kind
		if cursor < len(tokens) && tokens[cursor].text == "(" {
			kind = .CALL
		}
		if target, ok := resolve(p, chain[:]); ok && target != context_name {
			append(&p.file.refs, Ref{ctx = context_name, target = target, kind = kind, tok = token})
		}
		index = max(index + 1, cursor)
	}
}

add_inheritance_refs :: proc(p: ^Parser, context_name: string, tokens: []Token, tok: Token) {
	for token in tokens {
		if token.kind != .Ident {
			continue
		}
		if is_builtin_type(token.text) {
			continue
		}
		chain := []string{token.text}
		if target, ok := resolve(p, chain); ok {
			append(&p.file.refs, Ref{ctx = context_name, target = target, kind = .INHERITANCE, tok = tok})
			break
		}
	}
}

add_body_locals :: proc(local_names: ^map[string]bool, tokens: []Token) {
	for token, index in tokens {
		if token.kind != .Ident {
			continue
		}
		if index + 1 < len(tokens) && (tokens[index + 1].text == ":=" || tokens[index + 1].text == ":") {
			local_names^[token.text] = true
		}
		if token.text == "for" && index + 1 < len(tokens) && tokens[index + 1].kind == .Ident {
			local_names^[tokens[index + 1].text] = true
		}
	}
}

resolve :: proc(p: ^Parser, chain: []string) -> (string, bool) {
	if len(chain) == 0 {
		return "", false
	}
	if len(chain) == 1 {
		if is_builtin_type(chain[0]) {
			return "", false
		}
		return make_full_name(p.file.pkg, chain[0]), true
	}
	first := chain[0]
	for imp in p.file.imports {
		if first == imp.alias || (imp.alias == "" && first == imp.pkg) {
			rest := chain[1:]
			joined, _ := strings.join(rest, ".", context.allocator)
			return make_full_name(imp.pkg, joined), true
		}
	}
	joined, _ := strings.join(chain, ".", context.allocator)
	return make_full_name(p.file.pkg, joined), true
}

make_full_name :: proc(owner: string, name: string) -> string {
	if owner == "" {
		return name
	}
	if name == "" {
		return owner
	}
	return fmt.tprintf("%s.%s", owner, name)
}

import_package_name :: proc(path: string) -> string {
	p := path
	if idx := strings.index_byte(p, ':'); idx >= 0 {
		p = p[idx + 1:]
	}
	for strings.has_suffix(p, "/") || strings.has_suffix(p, "\\") {
		p = p[:len(p) - 1]
	}
	idx := strings.last_index_byte(p, '/')
	if idx < 0 {
		idx = strings.last_index_byte(p, '\\')
	}
	if idx >= 0 {
		p = p[idx + 1:]
	}
	return p
}

collect_until :: proc(p: ^Parser, stop1: string, stop2: string, stop_at_nl: bool) -> []Token {
	result := make([dynamic]Token, 0, 16)
	depth := 0
	for peek(p).kind != .Eof {
		token := peek(p)
		if depth == 0 && ((stop1 != "" && token.text == stop1) || (stop2 != "" && token.text == stop2)) {
			break
		}
		if depth == 0 && stop_at_nl && token.kind == .Newline {
			break
		}
		token = advance(p)
		if token.text == "(" || token.text == "[" || token.text == "{" {
			depth += 1
		} else if token.text == ")" || token.text == "]" || token.text == "}" {
			if depth == 0 {
				p.index -= 1
				break
			}
			depth -= 1
		}
		append(&result, token)
	}
	return result[:]
}

collect_symbols :: proc(paths: []string) -> map[string]Symbol_Kind {
	table := make(map[string]Symbol_Kind)
	for path in paths {
		data, err := os.read_entire_file_from_path(path, context.allocator)
		if err != nil {
			continue
		}
		info := parse_file(path, string(data), false)
		table[info.pkg] = .PACKAGE
		for decl in info.decls {
			table[decl.name] = decl.kind
		}
	}
	return table
}

package_files :: proc(directory: string) -> []string {
	files := make([dynamic]string, 0, 8)
	entries, err := os.read_directory_by_path(directory, -1, context.allocator)
	if err != nil {
		return files[:]
	}
	for entry in entries {
		if strings.has_suffix(entry.name, ".odin") {
			append(&files, join_path(directory, entry.name))
		}
	}
	return files[:]
}

join_path :: proc(a: string, b: string) -> string {
	if strings.has_suffix(a, "/") || strings.has_suffix(a, "\\") {
		return fmt.tprintf("%s%s", a, b)
	}
	return fmt.tprintf("%s/%s", a, b)
}

file_symbol_paths :: proc(source_path: string, imports: [dynamic]Import) -> []string {
	directory := filepath.dir(source_path)
	paths := make([dynamic]string, 0, 8)
	seen := make(map[string]bool)
	add_paths :: proc(paths: ^[dynamic]string, seen: ^map[string]bool, dir: string) {
		for path in package_files(dir) {
			key := strings.to_lower(path)
			if !seen[key] {
				seen[key] = true
				append(paths, path)
			}
		}
	}
	add_paths(&paths, &seen, directory)
	for imp in imports {
		if strings.has_prefix(imp.path, "core:") || strings.has_prefix(imp.path, "base:") || strings.has_prefix(imp.path, "vendor:") {
			continue
		}
		candidate := join_path(directory, imp.path)
		add_paths(&paths, &seen, candidate)
	}
	return paths[:]
}

Writer :: struct {
	handle:  rawptr,
	file_id: c.int,
	ids:     map[string]c.int,
	table:   map[string]Symbol_Kind,
}

to_cstr :: proc(s: string) -> cstring {
	cstr, err := strings.clone_to_cstring(s, context.temp_allocator)
	if err != nil {
		return nil
	}
	return cstr
}

writer_symbol :: proc(w: ^Writer, full: string, kind: Symbol_Kind, tok: Token, scope: Token, has_scope: bool, with_location: bool) -> c.int {
	if id, ok := w.ids[full]; ok {
		return id
	}
	id := sodb_symbol(w.handle, to_cstr(full), ".", c.int(kind))
	if id == 0 {
		return 0
	}
	w.ids[full] = id
	if with_location && w.file_id != 0 {
		sodb_symbol_location(w.handle, id, w.file_id, c.int(tok.line), c.int(tok.col), c.int(tok.end_line), c.int(tok.end_col))
		if has_scope {
			sodb_symbol_scope_location(w.handle, id, w.file_id, c.int(tok.line), c.int(tok.col), c.int(scope.end_line), c.int(scope.end_col))
		}
	}
	return id
}

writer_reference :: proc(w: ^Writer, context_id: c.int, target: string, kind: Ref_Kind, tok: Token) {
	symbol_kind := w.table[target]
	if symbol_kind == .TYPE && target not_in w.table {
		symbol_kind = .GLOBAL_VARIABLE
	}
	if _, ok := w.table[target]; !ok {
		switch kind {
		case .CALL:
			symbol_kind = .FUNCTION
		case .TYPE_USAGE:
			symbol_kind = .TYPEDEF
		case .INHERITANCE:
			symbol_kind = .STRUCT
		case .IMPORT:
			symbol_kind = .PACKAGE
		case .USAGE:
			symbol_kind = .GLOBAL_VARIABLE
		}
	}
	target_id := writer_symbol(w, target, symbol_kind, tok, Token{}, false, false)
	if target_id == 0 {
		return
	}
	ref := sodb_reference(w.handle, context_id, target_id, c.int(kind))
	if ref != 0 {
		sodb_reference_location(w.handle, ref, w.file_id, c.int(tok.line), c.int(tok.col), c.int(tok.end_line), c.int(tok.end_col))
	}
}

index_file :: proc(source_path: string, database_path: string, verbose: bool) -> int {
	data, err := os.read_entire_file_from_path(source_path, context.allocator)
	if err != nil {
		fmt.eprintfln("cannot read %s: %s", source_path, err)
		return 1
	}
	info := parse_file(source_path, string(data), true)
	table := collect_symbols(file_symbol_paths(source_path, info.imports))
	table[info.pkg] = .PACKAGE
	for decl in info.decls {
		if _, ok := table[decl.name]; !ok {
			table[decl.name] = decl.kind
		}
	}

	handle := sodb_open(to_cstr(database_path))
	if handle == nil {
		fmt.eprintln("cannot open database")
		return 1
	}
	defer sodb_close(handle)
	if sodb_begin(handle) == 0 {
		fmt.eprintfln("begin failed: %s", string(sodb_last_error(handle)))
		return 1
	}

	normalized := strings.replace_all(source_path, "\\", "/", context.allocator) or_else source_path
	file_id := sodb_file(handle, to_cstr(normalized))
	if file_id == 0 {
		fmt.eprintfln("record file failed: %s", string(sodb_last_error(handle)))
		return 1
	}
	sodb_file_language(handle, file_id, "odin")

	w := Writer{handle = handle, file_id = file_id, ids = make(map[string]c.int), table = table}
	package_tok := info.pkg_tok
	if !info.has_package {
		package_tok = Token{line = 1, col = 1, end_line = 1, end_col = 1}
	}
	package_id := writer_symbol(&w, info.pkg, .PACKAGE, package_tok, Token{}, false, true)

	for imp in info.imports {
		target_id := writer_symbol(&w, imp.pkg, .PACKAGE, imp.tok, Token{}, false, false)
		if target_id != 0 && package_id != 0 {
			ref := sodb_reference(handle, package_id, target_id, c.int(Ref_Kind.IMPORT))
			if ref != 0 {
				sodb_reference_location(handle, ref, file_id, c.int(imp.tok.line), c.int(imp.tok.col), c.int(imp.tok.end_line), c.int(imp.tok.end_col))
			}
		}
	}

	for decl in info.decls {
		writer_symbol(&w, decl.name, decl.kind, decl.tok, decl.scope, decl.has_scope, true)
	}
	for ref in info.refs {
		context_id, ok := w.ids[ref.ctx]
		if !ok {
			context_id = writer_symbol(&w, ref.ctx, w.table[ref.ctx], Token{line = 1, col = 1, end_line = 1, end_col = 1}, Token{}, false, false)
		}
		if context_id != 0 {
			writer_reference(&w, context_id, ref.target, ref.kind, ref.tok)
		}
	}

	if sodb_commit(handle) == 0 {
		fmt.eprintfln("commit failed: %s", string(sodb_last_error(handle)))
		return 1
	}
	if verbose {
		fmt.printfln("indexed %s: %d declarations, %d references", source_path, len(info.decls), len(info.refs))
	}
	return 0
}

arg_value :: proc(args: []string, prefix: string) -> string {
	for arg in args {
		if strings.has_prefix(arg, prefix) {
			return arg[len(prefix):]
		}
	}
	return ""
}

main :: proc() {
	args := os.args
	source_path := arg_value(args, "--source-file-path=")
	database_path := arg_value(args, "--database-file-path=")
	verbose := false
	for arg in args {
		if arg == "--verbose" {
			verbose = true
		}
	}
	if source_path == "" || database_path == "" {
		fmt.eprintln("usage: SourcetrailOdinIndexer index --source-file-path=... --database-file-path=...")
		os.exit(1)
	}
	code := index_file(source_path, database_path, verbose)
	os.exit(code)
}
