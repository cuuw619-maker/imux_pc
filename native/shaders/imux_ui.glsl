#version 330 core
in vec2 v_uv;
uniform vec4 u_accent;
uniform float u_time;
out vec4 fragColor;

void main() {
    float wave = 0.5 + 0.5 * sin(v_uv.x * 6.2831853 + u_time);
    fragColor = vec4(u_accent.rgb + 0.025 * wave, 1.0);
}
