#version 450
layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec4 color;
layout(location=0) out vec4 outColor;
void main() {
    gl_Position = vec4(position, 1.0);
    float lighting = 0.55 + 0.45 * max(dot(normalize(normal), normalize(vec3(-0.45,0.85,-0.35))), 0.0);
    outColor = vec4(color.rgb * lighting, 1.0);
}
