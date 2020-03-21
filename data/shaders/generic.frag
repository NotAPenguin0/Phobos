#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#define MAX_LIGHT_COUNT 32

layout(location = 0) in vec3 Normal;
layout(location = 1) in vec2 TexCoords;
layout(location = 2) in vec3 FragPos;
layout(location = 3) in vec3 ViewPos;

layout(location = 0) out vec4 FragColor;

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

layout(set = 0, binding = 2) uniform Lights {
    PointLight point_lights[MAX_LIGHT_COUNT];
    // other light types
    uint point_light_count;
} lights;


layout(set = 0, binding = 3) uniform sampler2D textures[];

layout(push_constant) uniform Indices {
    uint texture_index;
    uint transform_indices;
} indices;

vec3 apply_point_light(vec3 in_color, PointLight light) {
    vec3 ambient = in_color * light.ambient;
    vec3 norm = normalize(Normal);
    vec3 light_dir = normalize(light.position - FragPos);
    float diff = max(dot(norm, light_dir), 0.0);
    vec3 diffuse = in_color * diff * light.diffuse;

    vec3 view_dir = normalize(ViewPos - FragPos);
    vec3 reflect_dir = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 2);
    vec3 specular = in_color * spec * light.specular;

    return ambient + diffuse + specular;
}

void main() {
    vec3 color = texture(textures[indices.texture_index], TexCoords).rgb;
    for (uint i = 0; i < lights.point_light_count; ++i) {
        color = apply_point_light(color, lights.point_lights[i]);
    }
    FragColor = vec4(color, 1.0);
}