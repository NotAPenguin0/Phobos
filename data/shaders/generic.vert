#version 450

layout(location = 0) in vec3 iPos;
layout(location = 1) in vec2 iTexCoords;

layout(location = 0) out vec2 TexCoords;

layout(set = 0, binding = 0) uniform Matrices {
    mat4 projection_view;
    mat4 model;
} matrices;

void main() {
    TexCoords = iTexCoords;
    gl_Position = matrices.projection_view * matrices.model * vec4(iPos, 1.0);
}