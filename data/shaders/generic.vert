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

layout(std430, set = 0, binding = 1) buffer readonly TransformData {
    mat4 models[];
} transforms;

layout(push_constant) uniform Indices {
    uint texture_index;
    uint transform_index;
} indices;

void main() {
    mat4 model = transforms.models[indices.transform_index];
    Normal = mat3(transpose(inverse(model))) * iNormal; 
    TexCoords = iTexCoords;
    FragPos = vec3(model * vec4(iPos, 1.0));
    ViewPos = camera.cam_pos;

    gl_Position = camera.projection_view * model * vec4(iPos, 1.0);
}