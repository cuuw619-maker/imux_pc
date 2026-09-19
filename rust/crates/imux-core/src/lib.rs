pub mod world;

#[unsafe(no_mangle)]
pub extern "C" fn imux_rust_version() -> *const std::ffi::c_char {
    static VERSION: &[u8] = b"0.1.0-rust\0";
    VERSION.as_ptr().cast()
}

pub fn bootstrap() -> &'static str { "Imux Rust bootstrap" }

pub fn test_world_gravity() -> f32 { 12.0 }
