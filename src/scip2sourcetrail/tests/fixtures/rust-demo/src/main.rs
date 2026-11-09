mod shapes;

use crate::shapes::{Area, Square};

fn main() {
    let q = Square::new(2.0);
    println!("{}", total(&q));
}

fn total(q: &Square) -> f64 {
    q.area() + shapes::doubled(q) + std::f64::consts::PI
}
