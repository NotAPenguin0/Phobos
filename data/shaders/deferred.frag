#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) out vec4 gNormal;
layout (location = 1) out vec4 gAlbedoSpec;

layout(location = 0) in vec2 TexCoords;
layout(location = 1) in mat3 TBN;

layout(set = 0, binding = 2) uniform sampler2D textures[];

layout(push_constant) uniform Indices {
    uint transform_index;
    uint diffuse_index;
    uint specular_index;
    uint normal_index;
} indices;

// Deferred example adapted from learnopengl.com

void main() {    
    // Store the per-fragment normals into the gbuffer
    vec3 norm = texture(textures[indices.normal_index], TexCoords).rgb * 2.0 - 1.0;
    gNormal.xyz = normalize(TBN * norm) * 0.5 + 0.5;
//    gNormal.xyz = texture(textures[indices.normal_index], TexCoords).rgb;
    gNormal.w = 1.0;
    // and the diffuse per-fragment color
    gAlbedoSpec.rgb = texture(textures[indices.diffuse_index], TexCoords).rgb;
    // store specular intensity in gAlbedoSpec's alpha component
    gAlbedoSpec.a = texture(textures[indices.specular_index], TexCoords).r;
}