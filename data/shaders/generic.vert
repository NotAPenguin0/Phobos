#version 450

layout(location = 0) in vec3 iPos;
layout(location = 1) in vec3 iNormal;
layout(location = 2) in vec3 iTangent;
layout(location = 3) in vec2 iTexCoords;

layout(location = 0) out vec2 TexCoords;
layout(location = 1) out vec3 FragPos;
layout(location = 2) out vec3 ViewPos;
layout(location = 3) out mat3 TBN;
layout(location = 6) out vec3 Tangent;
layout(location = 7) out vec3 Normal;


layout(set = 0, binding = 0) uniform CameraData {
    mat4 projection_view;
    vec3 cam_pos;
} camera;

layout(std430, set = 0, binding = 1) buffer readonly TransformData {
    mat4 models[];
} transforms;

layout(push_constant) uniform Indices {
    uint transform_index;
    uint diffuse_index;
    uint specular_index;
    uint normal_index;
} indices;

vec3 calculate_tangent(vec3 n) {
	vec3 v = vec3(1.0, 0.0, 0.0);
	float d = dot(v, n);
	if (abs(d) < 1.0e-3) {
		v = vec3(0.0, 1.0, 0.0);
		d = dot(v, n);
	}
	return normalize(v - d * n);
}

void main() {
    mat4 model = transforms.models[indices.transform_index];
    mat3 normal = transpose(inverse(mat3(model)));
    vec3 T = normalize(normal * iTangent);
    vec3 N = normalize(normal * iNormal);
//    T = normalize(T - dot(T, N) * N);
    vec3 B = cross(N, T);
    TBN = mat3(T, B, N);
    TexCoords = iTexCoords;
    Tangent = iTangent;
    Normal = normal * iNormal;
    FragPos = vec3(model * vec4(iPos, 1.0));
    ViewPos = camera.cam_pos;

    gl_Position = camera.projection_view * model * vec4(iPos, 1.0);
}