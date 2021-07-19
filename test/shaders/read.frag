#version 450

layout(location = 0) in vec2 UV;

layout(location = 0) out vec4 Color;

layout(set = 0, binding = 0) uniform sampler2D image;

void main() {
    Color = vec4(texture(image, UV).rgb, 1);
}