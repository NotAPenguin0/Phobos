#version 450

layout(location = 0) in vec2 TexCoords;

layout(set = 0, binding = 0) uniform sampler2D gAlbedoSpec;

layout(location = 0) out vec4 FragColor;

void main() {
    // 0.1 is the ambient strength
    FragColor = vec4(texture(gAlbedoSpec, TexCoords).rgb * 0.01, 1.0);
}