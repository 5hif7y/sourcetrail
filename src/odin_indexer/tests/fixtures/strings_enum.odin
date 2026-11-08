package strings_enum

Color :: enum {
	Red,
	Green,
	Blue = Green,
}

State :: struct {
	first: int,
	second: int,
}

raw :: proc() -> string {
	return """
		This is text, not Odin:
		proc fake() {
			phantom := 42
			missing_call()
		}
	"""
}

offset :: proc() -> int {
	return offset_of(State, second)
}
