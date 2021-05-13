#version 450

layout(location = 0) in vec2 iPos; // Ignored, but I'm too lazy to set up a compute shader
layout(location = 1) in vec2 iUV;

layout(set = 0, binding = 1) writeonly buffer OutBuffer {
    vec3 color;
} out_buffer;

void main() {
    out_buffer.color = vec3(0, 0, 1);
}