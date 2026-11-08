package syntax

CF :: distinct f32

foreign libc {
	cacos :: proc(z: CF) -> CF ---
}

@private
PRIVATE_CONST :: 42

forced :: #force_inline proc(x: int) -> int {
	return x + 1
}

Generic :: struct($T: type) where T != i32 {
	value: T,
}

use_generic :: proc(v: Generic(int)) -> int {
	return v.value
}

when ODIN_OS == .Windows {
	platform :: "windows"
} else {
	platform :: "other"
}
