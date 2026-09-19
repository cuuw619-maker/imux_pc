#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct Vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

impl Vec3 {
    pub fn new(x: f32, y: f32, z: f32) -> Self { Self { x, y, z } }

    pub fn add_scaled(self, velocity: Vec3, dt: f32) -> Self {
        Self::new(
            self.x + velocity.x * dt,
            self.y + velocity.y * dt,
            self.z + velocity.z * dt,
        )
    }
}

pub fn integrate_player(position: Vec3, velocity: Vec3, dt: f32) -> (Vec3, Vec3) {
    let mut next_velocity = velocity;
    next_velocity.y -= 12.0 * dt;
    let mut next_position = position.add_scaled(next_velocity, dt);

    if next_position.y < 2.0 {
        next_position.y = 2.0;
        next_velocity.y = 0.0;
    }

    (next_position, next_velocity)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn player_stays_on_test_world_floor() {
        let (position, velocity) = integrate_player(
            Vec3::new(0.0, 2.0, 0.0),
            Vec3::new(0.0, -4.0, 0.0),
            0.5,
        );
        assert_eq!(position.y, 2.0);
        assert_eq!(velocity.y, 0.0);
    }
}
