#version 450

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 Color;

layout(set = 0, binding = 0) uniform sampler2D image;
layout(set = 0, binding = 1) uniform UniformBuffer {
    vec3 some_color;
} ubo;

void main() {
    Color = texture(image, UV);
    Color = vec4(ubo.some_color, 1);
}