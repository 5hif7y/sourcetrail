#+build windows, linux
#assert(true)

package main

import "core:fmt"

Color :: enum u8 {
	Red,
	Green,
	Blue,
}

Flags :: bit_set[Color]

Value :: union {
	int,
	string,
}

Id :: distinct int

Base :: struct {
	id: int,
}

User :: struct {
	using base: Base,
	name:      string,
	age:       int,
	favorite:  Color,
	flags:     Flags,
}

Packed :: bit_field u8 {
	low:  bool | 1,
	high: bool | 1,
}

@(private)
internal_value :: 42

greet :: proc(u: User) -> string {
	return fmt.tprintf("hello %s", u.name)
}

when ODIN_OS == .Windows {
	platform_name :: "windows"
} else {
	platform_name :: "other"
}
