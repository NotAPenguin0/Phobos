#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#define MAX_LIGHT_COUNT 32

layout(location = 0) in vec2 TexCoords;
layout(location = 1) in vec3 FragPos;
layout(location = 2) in vec3 ViewPos;
layout(location = 3) in mat3 TBN;

layout(location = 0) out vec4 FragColor;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
};

struct DirectionalLight {
    vec3 direction;
    vec3 color;
};

layout(set = 0, binding = 2) uniform Lights {
    PointLight point_lights[MAX_LIGHT_COUNT];
    DirectionalLight directional_lights[MAX_LIGHT_COUNT];
    uint point_light_count;
    uint directional_light_count;
} lights;


layout(set = 0, binding = 3) uniform sampler2D textures[];

layout(push_constant) uniform Indices {
    uint transform_index;
    uint diffuse_index;
    uint specular_index;
    uint normal_index;
} indices;

vec3 apply_point_light(vec3 norm, vec3 in_color, PointLight light) {
    vec3 ambient = in_color * 0.1f; // fixed ambient component for now
    vec3 light_dir = normalize(light.position - FragPos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = in_color * diff * light.color;

    vec3 view_dir = normalize(ViewPos - FragPos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 2);
    vec3 specular = in_color * spec * light.color * texture(textures[indices.specular_index], TexCoords).rgb;

    float dist = length(FragPos - light.position);
    float falloff = 1.0f / dist; // linear falloff :/

    return (ambient + diffuse + specular) * falloff * light.intensity;
}

vec3 apply_directional_light(vec3 norm, vec3 in_color, DirectionalLight light) {
    vec3 ambient = in_color * 0.1f; // fixed ambient component for now
    vec3 light_dir = normalize(-light.direction);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = in_color * diff * light.color;

    vec3 view_dir = normalize(ViewPos - FragPos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 2);
    vec3 specular = in_color * spec * light.color * texture(textures[indices.specular_index], TexCoords).rgb;

    return (ambient + diffuse + specular);
}

void main() {
    vec3 color = texture(textures[indices.diffuse_index], TexCoords).rgb;
    vec3 norm = normalize(texture(textures[indices.normal_index], TexCoords).rgb * 2.0 - 1.0);
    norm = normalize(TBN * norm);
    for (uint i = 0; i < lights.point_light_count; ++i) {
        color = apply_point_light(norm, color, lights.point_lights[i]);
    }
    for (uint i = 0; i < lights.directional_light_count; ++i) {
        color = apply_directional_light(norm, color, lights.directional_lights[i]);
    }
    FragColor = vec4(color, 1.0);
}