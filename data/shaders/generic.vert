#version 450

layout(location = 0) in vec3 iPos;
layout(location = 1) in vec2 iTexCoords;

layout(location = 0) out vec2 TexCoords;
layout(location = 1) out vec3 clr;

layout(set = 0, binding = 0) uniform Matrices {
    mat4 projection_view;
    mat4 model;
} matrices;

layout(std430, set = 0, binding = 1) buffer readonly InstanceData {
    mat4 models[];
} instances;

void main() {
    TexCoords = iTexCoords;
    if (gl_InstanceIndex == 0) {
        clr = vec3(1, 0, 0);
    } else {
        clr = vec3(0, 1, 0);
    }
    gl_Position = matrices.projection_view * instances.models[gl_InstanceIndex] * vec4(iPos, 1.0);
}