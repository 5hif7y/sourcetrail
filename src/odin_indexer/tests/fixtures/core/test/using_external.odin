package using_external

import "core:image"

read :: proc() -> int {
	header := (^image.Header)(nil)
	using header
	return width + height
}
