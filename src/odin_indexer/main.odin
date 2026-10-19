package main

import "core:c"
import "core:fmt"
import "core:os"
import "core:path/filepath"
import "core:strings"
import "core:time"

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
	sodb_unsolved_reference    :: proc(w: rawptr, source: c.int, kind: c.int, file: c.int, sl: c.int, sc: c.int, el: c.int, ec: c.int) -> c.int ---
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
	alias:      string,
	local_name: string, // alias, or the last path component when unaliased
	path:       string,
	pkg:        string, // declared package name (owner of its symbols)
	tok:        Token,
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
	tokens:      []Token,
	index:       int,
	full:        bool,
	file:        File_Info,
	generic:     map[string]bool,
	source_path: string,
}

MULTI_OPS := []string{"::", ":=", "->", "..<", "..=", "...", "==", "!=", "<=", ">=", "&&", "||", "<<", ">>", "&~", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^="}

is_keyword :: proc(s: string) -> bool {
	switch s {
	case "package", "import", "foreign", "proc", "struct", "union", "enum",
		"bit_set", "bit_field", "distinct", "map", "matrix", "using", "when",
		"where", "if", "else", "for", "in", "not_in", "switch", "case", "defer",
		"return", "break", "continue", "fallthrough", "or_return", "or_else",
		"or_break", "or_continue", "nil", "true", "false", "cast", "transmute",
		"type_of", "type_info_of", "size_of", "align_of", "offset_of", "context":
		return true
	}
	return false
}

is_builtin_type :: proc(s: string) -> bool {
	if len(s) > 3 && (s[0] == 'u' || s[0] == 'i' || s[0] == 'f') &&
		(strings.has_suffix(s, "le") || strings.has_suffix(s, "be")) {
		digits := s[1:len(s) - 2]
		all_digits := len(digits) > 0
		for ch in digits {
			if ch < '0' || ch > '9' {
				all_digits = false
				break
			}
		}
		if all_digits {
			return true
		}
	}
	switch s {
	case "int", "uint", "uintptr", "typeid", "rawptr", "string", "cstring", "cstring16", "bool",
		"b8", "b16", "b32", "b64", "rune", "any", "byte", "u8", "u16", "u32",
		"u64", "u128", "i8", "i16", "i32", "i64", "i128", "f16", "f32", "f64",
		"f128", "complex32", "complex64", "complex128", "quaternion64",
		"quaternion128", "quaternion256", "intle", "intbe", "uintle", "uintbe",
		"dynamic", "Maybe", "string16", "Objc_Block":
		return true
	}
	return false
}

// Builtins that are procedures rather than types. They are not declared in the
// indexed package, so treating them as unresolved would flood the database.
is_builtin_proc :: proc(s: string) -> bool {
	if strings.has_prefix(s, "ODIN_") {
		return true
	}
	switch s {
	case "len", "cap", "append", "make", "new", "delete", "clear", "copy",
		"min", "max", "abs", "clamp", "real", "imag", "quat", "complex", "conj",
		"expand_values", "raw_data", "auto_cast", "cast", "transmute", "type_of",
		"type_info_of", "size_of", "align_of", "offset_of", "swizzle",
		"is_nan", "is_inf", "is_finite", "nan", "inf", "soa_zip", "soa_unzip",
		"assert", "panic", "panic_contextless", "ensure", "ensure_contextless",
		"unimplemented", "unimplemented_contextless", "todo", "resize", "reserve",
		"new_clone", "free", "copy_slice", "map_insert", "delete_key",
		"append_elem", "non_zero_append_elem", "_append_elem", "_append_elems",
		"ordered_remove", "unordered_remove", "pop", "shrink", "quaternion",
		"jmag", "kmag", "non_zero_resize", "map_entry", "simd_shuffle",
		"free_all", "container_of", "inject_at",
		"offset_of_by_string", "typeid_of", "assert_contextless", "unreachable",
		"card", "make_slice":
		return true
	}
	return false
}

is_external_import_path :: proc(path: string) -> bool {
	return strings.has_prefix(path, "core:") || strings.has_prefix(path, "base:") ||
		strings.has_prefix(path, "vendor:")
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
		// Triple-quoted raw strings: C++/SQL/text embedded in Odin sources
		// must not be lexed as Odin identifiers.
		if ch == '"' && i + 2 < n && source[i + 1] == '"' && source[i + 2] == '"' {
			start_i, start_line, start_col := i, line, col
			advance(source, &i, &line, &col, 3)
			for i < n {
				if i + 2 < n && source[i] == '"' && source[i + 1] == '"' &&
					source[i + 2] == '"' {
					advance(source, &i, &line, &col, 3)
					break
				}
				advance(source, &i, &line, &col)
			}
			emit(&tokens, source, .Raw, start_i, start_line, start_col, i, line, col)
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

skip_until_body :: proc(p: ^Parser) {
	for peek(p).kind != .Eof && !at(p, "{") {
		advance(p)
	}
}

skip_newlines :: proc(p: ^Parser) {
	for peek(p).kind == .Newline {
		advance(p)
	}
}

skip_braced :: proc(tokens: []Token, open_index: int) -> int {
	depth := 0
	index := open_index
	for index < len(tokens) {
		if tokens[index].text == "{" {
			depth += 1
		} else if tokens[index].text == "}" {
			depth -= 1
			if depth == 0 {
				return index + 1
			}
		}
		index += 1
	}
	return index
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

// Skip one directive token (#optional_ok, #align(...), ...) without consuming
// the declaration that follows it on the same line.
skip_token_directive :: proc(p: ^Parser) {
	if !accept(p, "#") {
		return
	}
	if peek(p).kind == .Ident || peek(p).kind == .Keyword {
		advance(p)
	}
	if at(p, "(") {
		skip_balanced(p, "(", ")")
	}
}

skip_attribute :: proc(p: ^Parser) {
	if !accept(p, "@") {
		return
	}
	// Bare attributes (`@private`, `@export`, ...) carry a name; without this
	// the name was parsed as a declaration and the real declaration was lost.
	if peek(p).kind == .Ident || peek(p).kind == .Keyword {
		advance(p)
	}
	if at(p, "(") {
		skip_balanced(p, "(", ")")
	}
}

parse_file :: proc(path: string, source: string, full: bool) -> File_Info {
	p := Parser{
		tokens = lex(source),
		index = 0,
		full = full,
		generic = make(map[string]bool),
		source_path = path,
	}
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

package_name_cache: map[string]string

declared_package_name :: proc(directory: string) -> string {
	if package_name_cache == nil {
		package_name_cache = make(map[string]string)
	}
	if name, exists := package_name_cache[directory]; exists {
		return name
	}

	name := ""
	for path in package_files(directory) {
		data, err := os.read_entire_file_from_path(path, context.allocator)
		if err != nil {
			continue
		}
		tokens := lex(string(data))
		for index := 0; index + 1 < len(tokens); index += 1 {
			if tokens[index].text == "package" && tokens[index + 1].kind == .Ident {
				name = tokens[index + 1].text
				break
			}
		}
		if name != "" {
			break
		}
	}
	package_name_cache[directory] = name
	return name
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
		pkg := ""
		if is_external_import_path(path) {
			pkg = import_package_name(path)
		} else {
			// The directory name is not always the declared package name
			// (core/rexcode/isa/x86 declares `package rexcode_x86`).
			directory := normalize_path(join_path(filepath.dir(p.source_path), path))
			pkg = declared_package_name(directory)
			if pkg == "" {
				pkg = import_package_name(path)
			}
		}
		local_name := alias
		if local_name == "" {
			local_name = import_package_name(path)
		}
		append(&p.file.imports, Import{
			alias = alias,
			local_name = local_name,
			path = path,
			pkg = pkg,
			tok = path_token,
		})
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
			if at(p, "@") {
				skip_attribute(p)
				continue
			}
			if at(p, "#") {
				skip_token_directive(p)
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
			skip_token_directive(p)
			continue
		}
		if at(p, "@") {
			skip_attribute(p)
			continue
		}
		if at(p, "import") {
			parse_import(p)
			continue
		}
		if at(p, "foreign") {
			parse_foreign(p)
			continue
		}
		if at(p, "when") {
			parse_when(p)
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

	p.generic = make(map[string]bool)
	if accept(p, "::") {
		if len(names) == 1 && names[0].text == "_" {
			// `_ :: runtime` is an import/keep-alive marker, not a symbol.
			skip_to_nl(p)
			return
		}
		for name in names {
			parse_value(p, name)
		}
		return
	}
	if accept(p, ":=") {
		if len(names) == 1 && at(p, "proc") {
			// `handler := proc(...) { ... }` is a procedure value; parse the
			// literal instead of scanning its parameters as references.
			parse_proc(p, make_full_name(p.file.pkg, names[0].text), names[0])
			return
		}
		// Package-level variable with inferred type: `X := ...`.
		expr := collect_until(p, "=", ";", true)
		for name in names {
			full := make_full_name(p.file.pkg, name.text)
			append(&p.file.decls, Decl{name = full, kind = .GLOBAL_VARIABLE, tok = name})
			if p.full {
				scan_expr_tokens(p, expr, full, make(map[string]bool))
			}
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
		if at(p, "(") {
			generic_tokens := skip_balanced(p, "(", ")")
			collect_generic_params(p, generic_tokens)
		}
		append(&p.file.decls, Decl{name = full, kind = .STRUCT, tok = name})
		skip_newlines(p)
		if at(p, "where") {
			skip_until_body(p)
		}
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
		if at(p, "(") {
			generic_tokens := skip_balanced(p, "(", ")")
			collect_generic_params(p, generic_tokens)
		}
		append(&p.file.decls, Decl{name = full, kind = .UNION, tok = name})
		skip_newlines(p)
		if at(p, "where") {
			skip_until_body(p)
		}
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
		skip_newlines(p)
		if at(p, "where") {
			skip_until_body(p)
		}
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
			directive := advance(p).text
			if at(p, "(") {
				skip_balanced(p, "(", ")")
			}
			if directive == "type" {
				append(&p.file.decls, Decl{name = full, kind = .TYPEDEF, tok = name})
				skip_to_nl(p)
				return
			}
			// `name :: #force_inline proc ...`, `name :: #config(...)`, etc.
			next := peek(p)
			if next.text == "proc" {
				parse_proc(p, full, name)
				return
			}
			if next.text == "struct" || next.text == "union" || next.text == "enum" ||
				next.text == "bit_set" || next.text == "bit_field" || next.text == "distinct" {
				parse_value(p, name)
				return
			}
			expr := collect_until(p, "", ";", true)
			append(&p.file.decls, Decl{name = full, kind = .GLOBAL_VARIABLE, tok = name})
			if p.full {
				scan_expr_tokens(p, expr, full, make(map[string]bool))
			}
			return
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
			skip_token_directive(p)
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
		// `field: Type = default` keeps the default expression out of the type
		// reference set (it used to be scanned as a TYPE_USAGE).
		default_tokens: []Token
		eq_index := -1
		depth := 0
		for token, index in type_tokens {
			if token.text == "(" || token.text == "[" || token.text == "{" {
				depth += 1
			} else if token.text == ")" || token.text == "]" || token.text == "}" {
				depth -= 1
			} else if token.text == "=" && depth == 0 {
				eq_index = index
				break
			}
		}
		if eq_index >= 0 {
			default_tokens = type_tokens[eq_index + 1:]
			type_tokens = type_tokens[:eq_index]
		}
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
		if eq_index >= 0 && p.full {
			scan_expr_tokens(p, default_tokens, owner, make(map[string]bool))
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
			skip_token_directive(p)
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

// Inside an enum body, an unqualified name refers to a sibling variant
// (`Bitsquid = SJSON`), not to a package-level symbol. The regular resolver
// only knows package scope, so mark sibling variants as locals (to suppress
// unresolved references) and add the reference to the owning enum explicitly.
scan_enum_alias_expr :: proc(
	p: ^Parser,
	tokens: []Token,
	owner: string,
	context_name: string,
	variants: map[string]bool,
) {
	locals := make(map[string]bool)
	for name, _ in variants {
		locals[name] = true
	}
	scan_expr_tokens(p, tokens, context_name, locals)

	for token in tokens {
		if token.kind == .Ident && variants[token.text] {
			append(&p.file.refs, Ref{
				ctx = context_name,
				target = make_full_name(owner, token.text),
				kind = .USAGE,
				tok = token,
			})
		}
	}
}

parse_enum_variants :: proc(p: ^Parser, owner: string) {
	accept(p, "{")
	variants := make(map[string]bool)
	for peek(p).kind != .Eof && !at(p, "}") {
		if peek(p).kind == .Newline || at(p, ";") || at(p, ",") {
			advance(p)
			continue
		}
		if at(p, "#") {
			skip_token_directive(p)
			continue
		}
		if peek(p).kind == .Ident {
			variant := advance(p)
			full := make_full_name(owner, variant.text)
			append(&p.file.decls, Decl{name = full, kind = .ENUM_CONSTANT, tok = variant})
			if accept(p, "=") {
				expr := collect_until(p, ",", "}", true)
				if p.full {
					scan_enum_alias_expr(p, expr, owner, full, variants)
				}
			}
			variants[variant.text] = true
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

mark_generic_names :: proc(p: ^Parser, local_names: ^map[string]bool, tokens: []Token) {
	for token, index in tokens {
		if token.text == "$" && index + 1 < len(tokens) && tokens[index + 1].kind == .Ident {
			name := tokens[index + 1].text
			p.generic[name] = true
			if local_names != nil {
				local_names^[name] = true
			}
		}
	}
}

collect_generic_params :: proc(p: ^Parser, tokens: []Token) {
	for group in split_params(tokens) {
		colon := -1
		for token, index in group {
			if token.text == ":" {
				colon = index
				break
			}
		}
		limit := len(group)
		if colon >= 0 {
			limit = colon
		}
		for token in group[:limit] {
			if token.kind == .Ident {
				p.generic[token.text] = true
			}
		}
	}
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
	mark_generic_names(p, &local_names, param_tokens)
	groups := split_params(param_tokens)
	for group in groups {
		process_param_group(p, group, full, &local_names)
	}

	skip_newlines(p)
	for at(p, "#") {
		skip_token_directive(p)
	}
	if accept(p, "->") {
		result_tokens := collect_result_tokens(p)
		if p.full {
			scan_result_tokens(p, result_tokens, full, &local_names)
		}
	}
	// Results may be followed by directives (#optional_ok, ...) and newlines
	// before the body or `where` clause.
	for {
		if peek(p).kind == .Newline {
			advance(p)
			continue
		}
		if at(p, "#") {
			skip_token_directive(p)
			continue
		}
		break
	}
	if at(p, "where") {
		skip_until_body(p)
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
		add_using_fields(&local_names, body, p.file.decls)
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
		has_default := false
		for token in segment {
			if token.text == ":" {
				has_colon = true
			} else if token.text == ":=" || token.text == "=" {
				has_default = true
			}
		}
		if !has_colon {
			if has_default {
				// A complete defaulted parameter (`loc := #caller_location`)
				// must not be merged with the next parameter's name list.
				if len(pending) > 0 {
					append(&merged, pending[:])
					pending = make([dynamic]Token, 0, 8)
				}
				append(&merged, segment)
				continue
			}
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
		// Parameters may omit the type when they only have a default
		// (`allocator := context.allocator`); mark the names as locals.
		eq := -1
		for token, index in group {
			if token.text == "=" || token.text == ":=" {
				eq = index
				break
			}
		}
		if eq >= 0 {
			for token in group[:eq] {
				if token.kind == .Ident {
					local_names^[token.text] = true
				}
			}
			return
		}
		// Unnamed result type.
		if p.full {
			scan_tokens(p, group, full, local_names^, .TYPE_USAGE)
		}
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
		scan_tokens(p, type_tokens, full, local_names^, .TYPE_USAGE)
	}
}

scan_type_tokens :: proc(p: ^Parser, tokens: []Token, context_name: string) {
	scan_tokens(p, tokens, context_name, p.generic, .TYPE_USAGE)
}

scan_result_tokens :: proc(p: ^Parser, tokens: []Token, context_name: string, local_names: ^map[string]bool) {
	inner := tokens
	if len(inner) >= 2 && inner[0].text == "(" && inner[len(inner) - 1].text == ")" {
		inner = inner[1:len(inner) - 1]
	}
	for group in split_params(inner) {
		process_param_group(p, group, context_name, local_names)
	}
}

scan_expr_tokens :: proc(p: ^Parser, tokens: []Token, context_name: string, local_names: map[string]bool) {
	scan_tokens(p, tokens, context_name, local_names, .USAGE)
}

// A `proc(...)` type appears in struct fields, parameters and proc literals.
// Its parameter names must not become references; only their types do.
scan_proc_type :: proc(p: ^Parser, tokens: []Token, start: int, context_name: string, outer_locals: map[string]bool) -> int {
	index := start + 1
	for index < len(tokens) && tokens[index].text != "(" {
		if tokens[index].text == "{" {
			return index
		}
		index += 1
	}
	if index >= len(tokens) {
		return index
	}

	index += 1
	param_start := index
	depth := 0
	for index < len(tokens) {
		if tokens[index].text == "(" {
			depth += 1
		} else if tokens[index].text == ")" {
			if depth == 0 {
				break
			}
			depth -= 1
		}
		index += 1
	}
	param_tokens := tokens[param_start:index]
	if index < len(tokens) && tokens[index].text == ")" {
		index += 1
	}

	locals := make(map[string]bool)
	for name, _ in p.generic {
		locals[name] = true
	}
	for name, _ in outer_locals {
		locals[name] = true
	}
	mark_generic_names(p, &locals, param_tokens)
	for group in split_params(param_tokens) {
		process_param_group(p, group, context_name, &locals)
	}

	if index < len(tokens) && tokens[index].text == "->" {
		index += 1
		result_start := index
		result_depth := 0
		for index < len(tokens) {
			text := tokens[index].text
			if result_depth == 0 &&
				(text == "{" || text == ";" || text == "," || text == ")" || text == "}") {
				break
			}
			if text == "(" || text == "[" {
				result_depth += 1
			} else if text == ")" || text == "]" {
				if result_depth > 0 {
					result_depth -= 1
				}
			}
			index += 1
		}
		scan_result_tokens(p, tokens[result_start:index], context_name, &locals)
	}

	if index < len(tokens) && tokens[index].text == "{" {
		index += 1
		body_start := index
		depth = 1
		for index < len(tokens) {
			if tokens[index].text == "{" {
				depth += 1
			} else if tokens[index].text == "}" {
				depth -= 1
				if depth == 0 {
					break
				}
			}
			index += 1
		}
		body := tokens[body_start:index]
		if index < len(tokens) && tokens[index].text == "}" {
			index += 1
		}
		add_body_locals(&locals, body)
		add_using_fields(&locals, body, p.file.decls)
		scan_tokens(p, body, context_name, locals, .USAGE)
	}

	return index
}

is_import_local_name :: proc(p: ^Parser, name: string) -> bool {
	for imp in p.file.imports {
		if imp.local_name == name {
			return true
		}
	}
	return false
}

// `offset_of(Type, field)` only contains a symbol in its first argument; the
// second one is a struct field name.
scan_offset_of :: proc(
	p: ^Parser,
	tokens: []Token,
	start: int,
	context_name: string,
	locals: map[string]bool,
) -> int {
	index := start + 1
	if index >= len(tokens) || tokens[index].text != "(" {
		return index
	}
	index += 1
	depth := 0
	first_start := index
	first_end := index
	comma := -1
	for index < len(tokens) {
		text := tokens[index].text
		if text == "(" {
			depth += 1
		} else if text == ")" {
			if depth == 0 {
				break
			}
			depth -= 1
		} else if text == "," && depth == 0 && comma < 0 {
			comma = index
			first_end = index
		}
		index += 1
	}
	if comma < 0 {
		first_end = index
	}
	if first_end > first_start {
		scan_expr_tokens(p, tokens[first_start:first_end], context_name, locals)
	}
	if index < len(tokens) && tokens[index].text == ")" {
		index += 1
	}
	return index
}

scan_tokens :: proc(p: ^Parser, tokens: []Token, context_name: string, local_names: map[string]bool, default_kind: Ref_Kind) {
	index := 0
	brace_depth := 0
	paren_depth := 0
	// A local map variable can be mutated; a map parameter cannot. Maps are
	// references, so this also shares the caller's locals.
	locals := local_names
	if locals == nil {
		locals = make(map[string]bool)
	}
	for index < len(tokens) {
		token := tokens[index]
		if token.text == "proc" {
			index = scan_proc_type(p, tokens, index, context_name, locals)
			continue
		}
		if token.text == "foreign" {
			// Body-local foreign blocks (`when ... { foreign libc { ... } }`).
			// Their procedures are local names; do not treat the library name
			// or the declarations as references.
			cursor := index + 1
			if cursor < len(tokens) && tokens[cursor].kind == .Ident {
				cursor += 1
			}
			if cursor < len(tokens) && tokens[cursor].text == "{" {
				end := skip_braced(tokens, cursor)
				for i := cursor + 1; i + 1 < end; i += 1 {
					if tokens[i].kind == .Ident && tokens[i + 1].text == "::" {
						locals[tokens[i].text] = true
					}
				}
				index = end
				continue
			}
			index = cursor
			continue
		}
		if token.text == "struct" || token.text == "union" || token.text == "enum" ||
			token.text == "bit_field" {
			// Inline anonymous types: their field names are not references.
			cursor := index + 1
			for cursor < len(tokens) && tokens[cursor].text != "{" && tokens[cursor].text != ";" {
				cursor += 1
			}
			if cursor < len(tokens) && tokens[cursor].text == "{" {
				index = skip_braced(tokens, cursor)
				continue
			}
		}
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
		if token.text == "(" {
			paren_depth += 1
			index += 1
			continue
		}
		if token.text == ")" {
			if paren_depth > 0 {
				paren_depth -= 1
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
		// offset_of(Type, field): the second argument is a field name, not a
		// package symbol. Scan only the type argument. `offset_of` is lexed as
		// a keyword, so this must run before the kind guard below.
		if token.text == "offset_of" && index + 1 < len(tokens) &&
			tokens[index + 1].text == "(" {
			index = scan_offset_of(p, tokens, index, context_name, locals)
			continue
		}
		if token.kind != .Ident {
			index += 1
			continue
		}
		if token.text == "_" {
			index += 1
			continue
		}
		// A local shadows a builtin with the same name (`real`, `min`, ...).
		if locals[token.text] {
			cursor := index + 1
			for cursor + 1 < len(tokens) &&
				tokens[cursor].text == "." && tokens[cursor + 1].kind == .Ident {
				cursor += 2
			}
			index = cursor
			continue
		}
		if is_builtin_proc(token.text) {
			index += 1
			continue
		}
		// Bare import names (`_, _, _ :: intrinsics, strconv, math`) are not
		// package-level symbols; imported members still resolve below.
		if (index + 1 >= len(tokens) || tokens[index + 1].text != ".") &&
			(token.text == "intrinsics" || token.text == "builtin" ||
				is_import_local_name(p, token.text)) {
			index += 1
			continue
		}
		// Generic parameters ($T, $N, ...) are not package symbols.
		if index > 0 && tokens[index - 1].text == "$" {
			index += 1
			continue
		}
		// `.Red` enum shorthand and member chains rooted at a call result:
		// without type inference there is no owner to resolve, so skip the
		// whole dotted tail instead of inventing symbols.
		if index > 0 && tokens[index - 1].text == "." &&
			(index < 2 || tokens[index - 2].kind != .Ident) {
			cursor := index + 1
			for cursor + 1 < len(tokens) &&
				tokens[cursor].text == "." && tokens[cursor + 1].kind == .Ident {
				cursor += 2
			}
			index = cursor
			continue
		}
		// `ptr->member(...)` accesses a field of the pointed value.
		if index > 0 && tokens[index - 1].text == "->" {
			index += 1
			continue
		}
		// Composite-literal fields (`k = 4`) and named arguments
		// (`assert(condition = ...)`) are not symbol references.
		if (brace_depth > 0 || paren_depth > 0) && index + 1 < len(tokens) &&
			tokens[index + 1].text == "=" {
			skip := index == 0
			if !skip {
				previous := tokens[index - 1]
				skip = previous.text == "{" || previous.text == "," ||
					previous.text == "(" || previous.kind == .Newline
			}
			if skip {
				index += 1
				continue
			}
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
	if len(tokens) == 0 || tok.text == "_" {
		return
	}
	// Anonymous embeddings (`using _: struct { ... }`) have no named base.
	if tokens[0].text == "struct" || tokens[0].text == "union" || tokens[0].text == "#" ||
		tokens[0].text == "_" {
		return
	}
	for token, index in tokens {
		if token.kind != .Ident {
			continue
		}
		if is_builtin_type(token.text) {
			continue
		}
		chain := make([dynamic]string, 0, 4)
		append(&chain, token.text)
		cursor := index + 1
		for cursor + 1 < len(tokens) && tokens[cursor].text == "." &&
			tokens[cursor + 1].kind == .Ident {
			append(&chain, tokens[cursor + 1].text)
			cursor += 2
		}
		if target, ok := resolve(p, chain[:]); ok {
			append(&p.file.refs, Ref{ctx = context_name, target = target, kind = .INHERITANCE, tok = tok})
			break
		}
	}
}

add_using_fields :: proc(local_names: ^map[string]bool, tokens: []Token, decls: [dynamic]Decl) {
	has_using := false
	for token, index in tokens {
		if token.text == "using" && index + 1 < len(tokens) && tokens[index + 1].kind == .Ident {
			has_using = true
			break
		}
	}
	if !has_using {
		return
	}

	for decl in decls {
		if decl.kind != .FIELD {
			continue
		}
		name := decl.name
		if index := strings.last_index_byte(name, '.'); index >= 0 {
			name = name[index + 1:]
		}
		local_names^[name] = true
	}
}

add_body_locals :: proc(local_names: ^map[string]bool, tokens: []Token) {
	for token, index in tokens {
		if token.text == "for" {
			// `for i, v in ...` introduces locals; `for` is a keyword, so this
			// has to run before the identifier guard below.
			cursor := index + 1
			for cursor < len(tokens) && (tokens[cursor].text == "&" || tokens[cursor].text == "#") {
				cursor += 1
			}
			for cursor < len(tokens) && tokens[cursor].kind == .Ident {
				local_names^[tokens[cursor].text] = true
				if cursor + 1 < len(tokens) && tokens[cursor + 1].text == "," {
					cursor += 2
					for cursor < len(tokens) &&
						(tokens[cursor].text == "&" || tokens[cursor].text == "#") {
						cursor += 1
					}
					continue
				}
				break
			}
			continue
		}
		if token.text == "switch" {
			// `switch v in value`, `#partial switch &info in t.variant`.
			cursor := index + 1
			if cursor < len(tokens) && tokens[cursor].text == "&" {
				cursor += 1
			}
			if cursor < len(tokens) && tokens[cursor].kind == .Ident {
				local_names^[tokens[cursor].text] = true
			}
			continue
		}
		if token.text == "using" {
			// `using params` imports the fields of params into the scope.
			if index + 1 < len(tokens) && tokens[index + 1].kind == .Ident {
				local_names^[tokens[index + 1].text] = true
			}
			continue
		}
		if token.kind != .Ident {
			continue
		}
		if index + 1 < len(tokens) && (tokens[index + 1].text == ":=" || tokens[index + 1].text == ":") {
			// Mark every name in `a, b, c := ...`, not just the last one.
			cursor := index
			for cursor >= 0 {
				current := tokens[cursor]
				if current.kind == .Ident {
					local_names^[current.text] = true
				} else if current.text != "," {
					break
				}
				cursor -= 1
			}
		}
		if index + 1 < len(tokens) && tokens[index + 1].text == "::" {
			// Local constants, types and procedures.
			cursor := index
			for cursor >= 0 {
				current := tokens[cursor]
				if current.kind == .Ident {
					local_names^[current.text] = true
				} else if current.text != "," {
					break
				}
				cursor -= 1
			}
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
	// Pseudo-packages provided by the compiler, not by imports.
	if first == "intrinsics" || first == "builtin" {
		joined, _ := strings.join(chain, ".", context.allocator)
		return joined, true
	}
	for imp in p.file.imports {
		if first == imp.local_name {
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

// Resolve a reference target against the declarations known for the package.
// A member chain such as `global.struct_field.inner` is shortened to the
// longest declared prefix (`global`) instead of inventing symbols for every
// suffix. Targets in external collections are kept as opaque imported symbols.
resolve_ref_target :: proc(
	target: string,
	table: map[string]Symbol_Kind,
	external_pkgs: map[string]bool,
) -> (resolved: string, ok: bool, external: bool) {
	if target in table {
		return target, true, false
	}

	parts, _ := strings.split(target, ".", context.temp_allocator)
	if len(parts) == 0 {
		return "", false, false
	}
	if parts[0] == "intrinsics" || parts[0] == "builtin" || external_pkgs[parts[0]] {
		return target, true, true
	}

	// Do not shorten all the way to the package itself: a call like
	// `unknown.member()` must not become a call to the package symbol.
	for count := len(parts) - 1; count >= 2; count -= 1 {
		prefix, _ := strings.join(parts[:count], ".", context.temp_allocator)
		if prefix in table {
			return prefix, true, false
		}
	}
	return "", false, false
}

normalize_path :: proc(path: string) -> string {
	parts, _ := strings.split_multi(path, []string{"/", "\\"}, context.temp_allocator)
	stack := make([dynamic]string, 0, 8)
	for part in parts {
		if part == "" || part == "." {
			continue
		}
		if part == ".." {
			if len(stack) > 0 {
				pop(&stack)
			}
			continue
		}
		append(&stack, part)
	}
	joined, _ := strings.join(stack[:], "/", context.temp_allocator)
	if strings.has_prefix(path, "/") || strings.has_prefix(path, "\\") {
		return fmt.tprintf("/%s", joined)
	}
	return joined
}

import_package_name :: proc(path: string, source_directory: string = "") -> string {
	p := path
	if idx := strings.index_byte(p, ':'); idx >= 0 {
		p = p[idx + 1:]
	}

	combined := p
	if strings.has_prefix(p, ".") {
		combined = join_path(source_directory, p)
	}

	parts, _ := strings.split_multi(combined, []string{"/", "\\"}, context.temp_allocator)
	stack := make([dynamic]string, 0, 8)
	for part in parts {
		if part == "" || part == "." {
			continue
		}
		if part == ".." {
			if len(stack) > 0 {
				pop(&stack)
			}
			continue
		}
		append(&stack, part)
	}
	if len(stack) == 0 {
		return ""
	}
	return stack[len(stack) - 1]
}

// Collect a proc result type, stopping before the body, a `where` clause, a
// `---` foreign body marker, or a trailing directive such as #optional_ok.
collect_result_tokens :: proc(p: ^Parser) -> []Token {
	tokens := make([dynamic]Token, 0, 8)
	depth := 0
	for peek(p).kind != .Eof {
		token := peek(p)
		// A result type ends at the end of its line unless the next
		// non-empty line is the body, a `where` clause or a directive.
		// Without this, `Callback :: proc(...) -> T` followed by
		// `Holder :: struct {` consumes Holder's opening brace as the body.
		if depth == 0 && token.kind == .Newline {
			lookahead := p.index
			for lookahead < len(p.tokens) && p.tokens[lookahead].kind == .Newline {
				lookahead += 1
			}
			if lookahead < len(p.tokens) {
				next := p.tokens[lookahead]
				if next.text == "{" || next.text == "where" || next.text == "#" {
					advance(p)
					continue
				}
			}
			break
		}
		// `---` is lexed as three '-' tokens; the first one terminates a
		// foreign procedure declaration.
		if depth == 0 &&
			(token.text == "{" || token.text == "where" || token.text == "-" || token.text == "#") {
			break
		}
		advance(p)
		if token.text == "(" || token.text == "[" {
			depth += 1
		} else if token.text == ")" || token.text == "]" {
			if depth > 0 {
				depth -= 1
			}
		}
		append(&tokens, token)
	}
	return tokens[:]
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

Symbol_Info :: struct {
	id:           c.int,
	has_location: bool,
}

Writer :: struct {
	handle:  rawptr,
	file_id: c.int,
	ids:     map[string]Symbol_Info,
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
	if info, ok := w.ids[full]; ok {
		// In batch mode a symbol can be referenced before its defining file is
		// visited. Attach the location when the declaration finally arrives.
		if with_location && !info.has_location && w.file_id != 0 {
			sodb_symbol_location(w.handle, info.id, w.file_id, c.int(tok.line), c.int(tok.col), c.int(tok.end_line), c.int(tok.end_col))
			if has_scope {
				sodb_symbol_scope_location(w.handle, info.id, w.file_id, c.int(tok.line), c.int(tok.col), c.int(scope.end_line), c.int(scope.end_col))
			}
			info.has_location = true
			w.ids[full] = info
		}
		return info.id
	}
	id := sodb_symbol(w.handle, to_cstr(full), ".", c.int(kind))
	if id == 0 {
		return 0
	}
	has_location := with_location && w.file_id != 0
	w.ids[full] = Symbol_Info{id = id, has_location = has_location}
	if has_location {
		sodb_symbol_location(w.handle, id, w.file_id, c.int(tok.line), c.int(tok.col), c.int(tok.end_line), c.int(tok.end_col))
		if has_scope {
			sodb_symbol_scope_location(w.handle, id, w.file_id, c.int(tok.line), c.int(tok.col), c.int(scope.end_line), c.int(scope.end_col))
		}
	}
	return id
}

writer_reference :: proc(w: ^Writer, context_id: c.int, target: string, kind: Ref_Kind, tok: Token, external: bool) {
	symbol_kind := w.table[target]
	if !external && target not_in w.table {
		return
	}
	if external && target not_in w.table {
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

Table_Cache :: struct {
	tables: map[string]map[string]Symbol_Kind,
}

table_for_directory :: proc(cache: ^Table_Cache, directory: string) -> map[string]Symbol_Kind {
	normalized := normalize_path(directory)
	if table, exists := cache.tables[normalized]; exists {
		return table
	}
	table := collect_symbols(package_files(normalized))
	cache.tables[normalized] = table
	return table
}

merge_symbol_table :: proc(destination: ^map[string]Symbol_Kind, source: map[string]Symbol_Kind) {
	for name, kind in source {
		destination^[name] = kind
	}
}

build_symbol_table :: proc(source_path: string, imports: [dynamic]Import, cache: ^Table_Cache) -> map[string]Symbol_Kind {
	table := make(map[string]Symbol_Kind)
	directory := filepath.dir(source_path)
	merge_symbol_table(&table, table_for_directory(cache, directory))
	for imp in imports {
		if is_external_import_path(imp.path) {
			continue
		}
		merge_symbol_table(
			&table, table_for_directory(cache, normalize_path(join_path(directory, imp.path))))
	}
	return table
}

index_files :: proc(source_paths: []string, database_path: string, verbose: bool, timing: bool, report_unsolved: bool, debug_refs: bool) -> int {
	if len(source_paths) == 0 {
		fmt.eprintln("no source files")
		return 1
	}

	t_request := time.now()
	handle := sodb_open(to_cstr(database_path))
	if handle == nil {
		fmt.eprintln("cannot open database")
		return 1
	}
	defer sodb_close(handle)
	t_open := time.now()
	if sodb_begin(handle) == 0 {
		fmt.eprintfln("begin failed: %s", string(sodb_last_error(handle)))
		return 1
	}
	t_begin := time.now()

	cache := Table_Cache{tables = make(map[string]map[string]Symbol_Kind)}
	w := Writer{handle = handle, ids = make(map[string]Symbol_Info)}
	exit_code := 0

	for source_path in source_paths {
		t_start := time.now()
		data, err := os.read_entire_file_from_path(source_path, context.allocator)
		if err != nil {
			fmt.eprintfln("cannot read %s: %s", source_path, err)
			exit_code = 1
			continue
		}
		info := parse_file(source_path, string(data), true)
		t_parse := time.now()
		if debug_refs {
			fmt.eprintfln("file %s package=%s imports=%d", source_path, info.pkg, len(info.imports))
			for imp in info.imports {
				fmt.eprintfln("  import alias=\"%s\" package=\"%s\" path=\"%s\"", imp.alias, imp.pkg, imp.path)
			}
		}
		table := build_symbol_table(source_path, info.imports, &cache)
		table[info.pkg] = .PACKAGE
		for decl in info.decls {
			if _, exists := table[decl.name]; !exists {
				table[decl.name] = decl.kind
			}
		}
		t_symbols := time.now()

		normalized := strings.replace_all(source_path, "\\", "/", context.allocator) or_else source_path
		file_id := sodb_file(handle, to_cstr(normalized))
		if file_id == 0 {
			fmt.eprintfln("record file failed: %s", string(sodb_last_error(handle)))
			exit_code = 1
			continue
		}
		sodb_file_language(handle, file_id, "odin")
		if debug_refs {
			fmt.eprintfln("  file_id=%d", file_id)
		}
		t_file := time.now()

		w.file_id = file_id
		w.table = table
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
		t_decls := time.now()

		external_pkgs := make(map[string]bool)
		for imp in info.imports {
			if is_external_import_path(imp.path) {
				external_pkgs[imp.pkg] = true
			}
		}
		for ref in info.refs {
			context_id: c.int = 0
			if context_info, exists := w.ids[ref.ctx]; exists {
				context_id = context_info.id
			} else {
				context_id = package_id
			}
			if context_id == 0 {
				continue
			}
			target, resolved, external := resolve_ref_target(ref.target, w.table, external_pkgs)
			if debug_refs {
				state := "unsolved"
				if resolved {
					state = external ? "external" : "resolved"
				}
				fmt.eprintfln("  ref %d:%d %s -> %s (%s)", ref.tok.line, ref.tok.col, ref.target, target, state)
			}
			if resolved {
				writer_reference(&w, context_id, target, ref.kind, ref.tok, external)
			} else if report_unsolved {
				sodb_unsolved_reference(
					w.handle, context_id, c.int(ref.kind), w.file_id,
					c.int(ref.tok.line), c.int(ref.tok.col),
					c.int(ref.tok.end_line), c.int(ref.tok.end_col),
				)
			}
		}
		t_refs := time.now()

		if verbose {
			fmt.printfln("indexed %s: %d declarations, %d references", source_path, len(info.decls), len(info.refs))
		}
		if timing {
			fmt.eprintfln(
				"timing %s: parse=%.1f symbols=%.1f file=%.1f decls=%.1f refs=%.1f total=%.1f (ms)",
				filepath.base(source_path),
				time.duration_milliseconds(time.diff(t_start, t_parse)),
				time.duration_milliseconds(time.diff(t_parse, t_symbols)),
				time.duration_milliseconds(time.diff(t_symbols, t_file)),
				time.duration_milliseconds(time.diff(t_file, t_decls)),
				time.duration_milliseconds(time.diff(t_decls, t_refs)),
				time.duration_milliseconds(time.diff(t_start, t_refs)),
			)
		}
	}

	if sodb_commit(handle) == 0 {
		fmt.eprintfln("commit failed: %s", string(sodb_last_error(handle)))
		return 1
	}
	t_commit := time.now()
	if timing {
		fmt.eprintfln(
			"timing total: files=%d open=%.1f begin=%.1f commit=%.1f total=%.1f (ms)",
			len(source_paths),
			time.duration_milliseconds(time.diff(t_request, t_open)),
			time.duration_milliseconds(time.diff(t_open, t_begin)),
			time.duration_milliseconds(time.diff(t_begin, t_commit)),
			time.duration_milliseconds(time.diff(t_request, t_commit)),
		)
	}
	return exit_code
}

arg_value :: proc(args: []string, prefix: string) -> string {
	for arg in args {
		if strings.has_prefix(arg, prefix) {
			return arg[len(prefix):]
		}
	}
	return ""
}

arg_values :: proc(args: []string, prefix: string) -> []string {
	values := make([dynamic]string, 0, 8)
	for arg in args {
		if strings.has_prefix(arg, prefix) {
			append(&values, arg[len(prefix):])
		}
	}
	return values[:]
}

main :: proc() {
	args := os.args
	source_paths := arg_values(args, "--source-file-path=")
	database_path := arg_value(args, "--database-file-path=")
	verbose := false
	timing := false
	// Unresolved references are opt-in: by default the indexer omits them, like
	// the Python PoC, instead of adding thousands of edges to one "unsolved"
	// node. Use --unsolved when auditing parser coverage.
	report_unsolved := false
	debug_refs := false
	for arg in args {
		if arg == "--verbose" {
			verbose = true
		}
		if arg == "--timing" {
			timing = true
		}
		if arg == "--unsolved" {
			report_unsolved = true
		}
		if arg == "--no-unsolved" {
			report_unsolved = false
		}
		if arg == "--debug-refs" {
			debug_refs = true
		}
	}
	if len(source_paths) == 0 || database_path == "" {
		fmt.eprintln("usage: SourcetrailOdinIndexer index --source-file-path=... [--source-file-path=...] --database-file-path=...")
		os.exit(1)
	}
	code := index_files(source_paths, database_path, verbose, timing, report_unsolved, debug_refs)
	os.exit(code)
}
