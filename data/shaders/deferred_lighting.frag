#version 450

#define MAX_LIGHT_COUNT 32

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 GBufferTexCoords;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 projection_view;
    mat4 inverse_projection;
    mat4 inverse_view;
    vec3 position;
} camera;

struct PointLight {
    // First 3 values have the position, last has the range of the light (= intensity)
    vec4 transform;
    vec3 color;
};

layout(set = 0, binding = 1) buffer readonly PointLights {
   PointLight lights[];
} lights;


layout(set = 0, binding = 2) uniform sampler2D gDepth;
layout(set = 0, binding = 3) uniform sampler2D gNormal;
layout(set = 0, binding = 4) uniform sampler2D gAlbedoSpec;


layout(push_constant) uniform Indices {
    uint light;
} indices;

// adapted https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value
// Note that we don't remap the depth value since the vk depth range goes from 0 to 1, not from -1 to 1 like in OpenGL
vec3 WorldPosFromDepth(float depth) {
    float z = depth;

    vec4 clipSpacePosition = vec4(GBufferTexCoords * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = camera.inverse_projection * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = camera.inverse_view * viewSpacePosition;

    return worldSpacePosition.xyz;
}


vec3 apply_point_light(vec3 norm, vec3 in_color, PointLight light, vec3 WorldPos, float Specular) {
    vec3 light_dir = normalize(light.transform.xyz - WorldPos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = in_color * diff * light.color;

    vec3 view_dir = normalize(camera.position - WorldPos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 2);
    vec3 specular = in_color * spec * light.color * Specular;

    float dist = length(WorldPos - light.transform.xyz);
    float falloff = 1.0f / (dist * dist);

    return (diffuse + specular) * falloff * light.transform.w; // w component is range = intensity
}

void main() {             
    // Adapted from learnopengl.com

    // retrieve data from gbuffer
    vec3 Normal = texture(gNormal, GBufferTexCoords).rgb;
    vec4 AlbedoSpec = texture(gAlbedoSpec, GBufferTexCoords);
    vec3 Diffuse = AlbedoSpec.rgb;
    float Specular = AlbedoSpec.a;

    float depth = texture(gDepth, GBufferTexCoords).r;
    vec3 WorldPos = WorldPosFromDepth(depth);
    
    vec3 color = Diffuse;
    vec3 norm = Normal * 2.0 - 1.0;
    
    color = apply_point_light(norm, color, lights.lights[indices.light], WorldPos, Specular);

    FragColor = vec4(color, 1.0);
}


