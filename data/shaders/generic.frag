#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec2 TexCoords;

layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 2) uniform sampler2D textures[];

layout(push_constant) uniform TextureIndex {
    uint index;
} texture_index;

void main() {
    FragColor = texture(textures[texture_index.index], TexCoords);
}