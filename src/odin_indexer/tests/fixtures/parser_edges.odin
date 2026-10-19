package parser_edges

Error :: enum {
	None,
}

Header :: struct {
	width:  int,
	height: int,
}

Handler :: #type proc(
	value: int,
) -> (n: int, err: Error)

MAX :: min(#config(PARSER_EDGES_MAX, 16), 32)

read_header :: proc() -> (int, Error) {
	header := (^Header)(nil)
	using header

	@thread_local
	scratch: [4]int
	_ = scratch

	return width + height, .None
}

use_builtins :: proc(values: ^[dynamic]int, lookup: ^map[string]int) {
	append_elems(values, 1, 2)
	remove_range(values, 0, 1)
	clear_map(lookup)
}
