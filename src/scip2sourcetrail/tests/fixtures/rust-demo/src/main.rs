mod shapes;

use crate::shapes::{doubled, Area, Square};

fn main() {
    let q = Square::new(2.0);
    println!("{}", total(&q));
}

fn total(q: &Square) -> f64 {
    q.area() + doubled(q) + std::f64::consts::PI
}
