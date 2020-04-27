#version 450

layout(location = 0) out vec4 FragColor;

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
    FragColor = vec4(light.color.rgb, 1.0);
}