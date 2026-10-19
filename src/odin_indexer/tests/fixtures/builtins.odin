package builtins

Thing :: struct {
	value: int,
}

make_things :: proc() -> [dynamic]Thing {
	items := make_dynamic_array([dynamic]Thing)
	append(&items, Thing{value = 1})
	return items
}

consume :: proc(items: ^[dynamic]Thing) -> Thing {
	return pop_front(items)
}

use_builtins :: proc() -> int {
	items := make_things()
	first := consume(&items)
	assign_at(&items, 0, Thing{value = first.value + 1})

	values := make_map(map[string]int)
	defer delete_map(values)
	map_upsert(&values, "first", first.value)

	temporary := make_multi_pointer([^]u8, 4)
	_ = temporary

	delete_dynamic_array(items)
	return first.value
}
