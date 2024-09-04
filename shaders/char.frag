#version 450
#extension GL_GOOGLE_include_directive : require

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec2 inUV;

// Scene bindings
layout (set = 0, binding = 0) uniform UBO {
	mat4 projection;
	vec4 lightDir;
	vec3 camPos;
} ubo;

//pushed descriptor set
layout (set = 1, binding = 1) uniform sampler2D normalTexture;
layout (set = 1, binding = 2) uniform sampler2D metalRoughTexture;
layout (set = 1, binding = 3) uniform sampler2D albedoTexture;

// layout (push_constant) uniform PushConstants {
// 	int materialIndex;
// } pushConstants;

layout (location = 0) out vec4 outColor;

const float M_PI = 3.141592653589793;

void main()
{
    vec3 albedo = texture(albedoTexture, inUV).xyz;
    float rough = texture(metalRoughTexture, inUV).y;
    vec3 normal = texture(normalTexture, inUV).xyz; 

	outColor = vec4(albedo, 1.0);
}