#version 450

layout(location = 0) in vec3 TexCoords;

layout(set = 0, binding = 1) uniform samplerCube skybox;

layout(location = 0) out vec4 FragColor;

void main() {
	FragColor = vec4(texture(skybox, TexCoords).rgb, 1.0);
	// we want the skybox to render behind everything
	gl_FragDepth = 1.0;
}