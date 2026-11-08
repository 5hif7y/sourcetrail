package globals

Point :: struct {
	x: int,
	y: int,
}

Foo :: struct {
	p: Point,
}

g: Point
f: Foo
count: int

use_globals :: proc() {
	g.x = 1
	f.p.y = 2
	count += 1
}
