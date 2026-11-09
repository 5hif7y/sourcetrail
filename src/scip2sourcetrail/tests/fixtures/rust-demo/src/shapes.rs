pub trait Area {
    fn area(&self) -> f64;
}

pub struct Square {
    pub side: f64,
}

impl Square {
    pub fn new(side: f64) -> Self {
        Square { side }
    }
}

impl Area for Square {
    fn area(&self) -> f64 {
        let s = self.side;
        s * s
    }
}

macro_rules! twice {
    ($e:expr) => {
        $e * 2.0
    };
}

pub fn doubled(q: &Square) -> f64 {
    twice!(q.area())
}
