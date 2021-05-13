#version 450

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 Color;

layout(set = 0, binding = 1) readonly buffer InBuffer {
    vec3 color;
} in_buffer;

void main() {
    Color = vec4(in_buffer.color, 1);
}