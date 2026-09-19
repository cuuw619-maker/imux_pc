struct UiUniforms {
    accent: vec4f,
    time: f32,
};

@group(0) @binding(0) var<uniform> ui: UiUniforms;

struct VertexOutput {
    @builtin(position) position: vec4f,
    @location(0) uv: vec2f,
};

@fragment
fn main(input: VertexOutput) -> @location(0) vec4f {
    let wave = 0.5 + 0.5 * sin(input.uv.x * 6.2831853 + ui.time);
    return vec4f(ui.accent.rgb + vec3f(0.025 * wave), 1.0);
}
