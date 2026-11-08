package proctypes

Callback :: proc(a, b: int) -> (res: int, ok: bool)

Holder :: struct {
	cmp: proc(a, b: int) -> bool,
	swap: proc(q: []int, i, j: int),
}

make_callback :: proc() -> Callback {
	return proc(a, b: int) -> (res: int, ok: bool) {
		return a + b, true
	}
}
