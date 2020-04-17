#version 450

layout (location = 0) in vec3 iPos;
layout (location = 1) in vec2 iTexCoords;

layout(location = 0) out vec2 TexCoords;

void main() {
    TexCoords = iTexCoords;
    gl_Position = vec4(iPos, 1.0);
}