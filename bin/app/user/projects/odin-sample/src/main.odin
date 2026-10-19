package main

import "core:fmt"
import m "mathx"

main :: proc() {
	u := User{name = "Ada", age = 36, favorite = Color.Red}
	v: Value = 42
	total := m.add(u.age, m.mul(2, 3))
	fmt.println(greet(u), total, v)
}
