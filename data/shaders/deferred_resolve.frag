#version 450

#define MAX_LIGHT_COUNT 32

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 TexCoords;

layout(set = 0, binding = 0) uniform sampler2D gDepth;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedoSpec;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
};

struct DirectionalLight {
    vec3 direction;
    vec3 color;
};

layout(set = 0, binding = 3) uniform Lights {
    PointLight point_lights[MAX_LIGHT_COUNT];
    DirectionalLight directional_lights[MAX_LIGHT_COUNT];
    uint point_light_count;
    uint directional_light_count;
} lights;

layout(set = 0, binding = 4) uniform CameraData {
    mat4 inverse_projection;
    mat4 inverse_view;
    vec3 cam_pos;
} camera;


// https://stackoverflow.com/questions/32227283/getting-world-position-from-depth-buffer-value
vec3 WorldPosFromDepth(float depth) {
    float z = depth * 2.0 - 1.0;

    vec4 clipSpacePosition = vec4(TexCoords * 2.0 - 1.0, z, 1.0);
    vec4 viewSpacePosition = camera.inverse_projection * clipSpacePosition;

    // Perspective division
    viewSpacePosition /= viewSpacePosition.w;

    vec4 worldSpacePosition = camera.inverse_view * viewSpacePosition;

    return worldSpacePosition.xyz;
}


vec3 apply_point_light(vec3 norm, vec3 in_color, PointLight light, vec3 WorldPos, float Specular) {
    vec3 light_dir = normalize(light.position - WorldPos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = in_color * diff * light.color;

    vec3 view_dir = normalize(camera.cam_pos - WorldPos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 2);
    vec3 specular = in_color * spec * light.color * Specular;

    float dist = length(WorldPos - light.position);
    float falloff = 1.0f / dist; // linear falloff :/

    return (diffuse + specular) * falloff * light.intensity;
}

vec3 apply_directional_light(vec3 norm, vec3 in_color, DirectionalLight light, vec3 WorldPos, float Specular) {
    vec3 light_dir = normalize(-light.direction);
    float diff = max(dot(light_dir, norm), 0.0);
    vec3 diffuse = diff * light.color * in_color;

    vec3 view_dir = normalize(camera.cam_pos - WorldPos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(norm, halfway_dir), 0.0), 32.0);
    vec3 specular = in_color * spec * light.color * Specular;

    return (diffuse + specular);
}

void main() {             
    // Adapted from learnopengl.com

    // retrieve data from gbuffer
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec4 AlbedoSpec = texture(gAlbedoSpec, TexCoords);
    vec3 Diffuse = AlbedoSpec.rgb;
    float Specular = AlbedoSpec.a;

    float depth = texture(gDepth, TexCoords).r;
    vec3 WorldPos = WorldPosFromDepth(depth);
    
    // then calculate lighting as usual
    vec3 color = Diffuse;
    vec3 ambient = 0.1 * Diffuse; // fixed ambient component
    vec3 norm = Normal * 2.0 - 1.0;
    for (uint i = 0; i < lights.point_light_count; ++i) {
        color = apply_point_light(norm, color, lights.point_lights[i], WorldPos, Specular);
    }
    for (uint i = 0; i < lights.directional_light_count; ++i) {
        color = apply_directional_light(norm, color, lights.directional_lights[i], WorldPos, Specular);
    }
    FragColor = vec4(color + ambient, 1.0);
}

