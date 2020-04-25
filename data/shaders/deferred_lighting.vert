#version 450

layout (location = 0) in vec3 iPos;


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

layout(push_constant) uniform PC {
    uint light_index;
    uvec2 screen_size;
} pc;

void main() {
    PointLight light = lights.lights[pc.light_index];
    vec4 light_world = vec4(light.transform.w * iPos + light.transform.xyz, 1.0);
    vec4 light_clip = camera.projection_view * light_world;
    gl_Position = light_clip;
    // Apply perspective division
//    vec3 light_ndc = light_clip.xyz / light_clip.w;
    // Do viewport transform and ignore z value
//    GBufferTexCoords = light_ndc.xy * 0.5 + 0.5;
}