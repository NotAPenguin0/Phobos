#version 450

layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec3 clr;

layout(location = 0) out vec4 FragColor;

void main() {
    FragColor = vec4(1, 0, 0, 1);
    FragColor = vec4(clr, 1);
}