#version 450

layout(location = 0) in vec3 iPos;

layout(location = 0) out vec3 TexCoords;

layout(set = 0, binding = 0) uniform Transform {
    mat4 projection;
} transform;

void main() {
    TexCoords = iPos;
    gl_Position = transform.projection * vec4(iPos, 1.0);
}