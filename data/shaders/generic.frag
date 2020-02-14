#version 450

layout(location = 0) in vec2 TexCoords;

layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(TexCoords, 0, 1);
}