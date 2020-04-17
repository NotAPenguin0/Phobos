#version 450

#define MAX_LIGHT_COUNT 32

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 TexCoords;

layout(set = 0, binding = 0) uniform sampler2D gPosition;
layout(set = 0, binding = 1) uniform sampler2D gNormal;
layout(set = 0, binding = 2) uniform sampler2D gAlbedoSpec;

struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

layout(set = 0, binding = 3) uniform Lights {
    PointLight point_lights[MAX_LIGHT_COUNT];
    // insert other light types here
    uint point_light_count;
} lights;

layout(set = 0, binding = 4) uniform CameraData {
    mat4 projection_view;
    vec3 cam_pos;
} camera;

void main() {             
    // Adapted from learnopengl.com

    // retrieve data from gbuffer
    vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec3 Diffuse = texture(gAlbedoSpec, TexCoords).rgb;
    float Specular = texture(gAlbedoSpec, TexCoords).a;
    
    // then calculate lighting as usual
    vec3 lighting  = Diffuse * 0.1; // hard-coded ambient component
    vec3 view_dir  = normalize(camera.cam_pos - FragPos);
    for(int i = 0; i < lights.point_light_count; ++i) {
        // calculate distance between light source and current fragment
        float dist = length(lights.point_lights[i].position - FragPos);
        // diffuse
        vec3 light_dir = normalize(lights.point_lights[i].position - FragPos);
        vec3 diffuse = max(dot(Normal, light_dir), 0.0) * Diffuse * lights.point_lights[i].diffuse;
        // specular
        vec3 halfway_dir = normalize(light_dir + view_dir);  
        float spec = pow(max(dot(Normal, halfway_dir), 0.0), 16.0); // hardcoded
        vec3 specular = lights.point_lights[i].specular * spec * Specular;
        // attenuation
        float attenuation = 1.0;
        diffuse *= attenuation;
        specular *= attenuation;
        lighting += diffuse + specular;
    }    
    FragColor = vec4(lighting, 1.0);
}

