#version 450

layout(location = 0) in vec3 iPos;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec2 iTexCoords;

layout(location = 0) out vec3 Normal;
layout(location = 1) out vec2 TexCoords;
layout(location = 2) out vec3 FragPos;
layout(location = 3) out vec3 ViewPos;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 projection_view;
    vec3 cam_pos;
} camera;

layout(std430, set = 0, binding = 1) buffer readonly InstanceData {
    mat4 models[];
} instances;

void main() {
    mat4 model = instances.models[gl_InstanceIndex];
    Normal = mat3(transpose(inverse(model))) * iNormal; 
    TexCoords = iTexCoords;
    FragPos = vec3(model * vec4(iPos, 1.0));
    ViewPos = camera.cam_pos;

    gl_Position = camera.projection_view * model * vec4(iPos, 1.0);
}