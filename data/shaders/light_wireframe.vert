#version 450

layout(location = 0) in vec3 iPos;

// Same inputs as the normal lighting pass so we don't have to create new buffers

layout(set = 0, binding = 0) uniform CameraData {
    mat4 projection_view;
    mat4 inverse_projection;
    mat4 inverse_view;
    vec3 position;
} camera;

struct PointLight {
    // First 3 values have the position, last has the radius of the light
    vec4 transform;
    // first 3 values are the color, last value has the intensity of the light
    vec4 color;
};

layout(set = 0, binding = 1) buffer readonly PointLights {
   PointLight lights[];
} lights;

layout(push_constant) uniform PC {
    uint light_index;
} pc;

void main() {
    PointLight light = lights.lights[pc.light_index];
    vec4 light_world = vec4(light.transform.w * iPos + light.transform.xyz, 1.0);
    vec4 light_clip = camera.projection_view * light_world;
    gl_Position = light_clip;
}