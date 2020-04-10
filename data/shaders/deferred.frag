#version 450

#extension GL_EXT_nonuniform_qualifier : enable

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gAlbedoSpec;

layout(location = 0) in vec3 FragPos;
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in vec3 Normal;

layout(set = 0, binding = 2) uniform sampler2D textures[];

layout(push_constant) uniform Indices {
    uint texture_index;
    uint transform_indices;
} indices;

// Deferred example adapted from learnopengl.com

void main() {    
    // store the fragment position vector in the first gbuffer texture
    gPosition = vec4(FragPos, 1.0);
    // also store the per-fragment normals into the gbuffer
    gNormal = vec4(normalize(Normal), 1.0);
    // and the diffuse per-fragment color
    gAlbedoSpec.rgb = texture(textures[indices.texture_index], TexCoords).rgb;
    // store specular intensity in gAlbedoSpec's alpha component
    gAlbedoSpec.a = 1.0f; // No specular texture support yet
}