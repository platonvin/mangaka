#version 450

layout(set = 0, binding = 0) uniform sampler2D depth_buffer;

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;

layout (location = 0) out vec3 outNormal;

// Scene bindings
layout (set = 0, binding = 1) uniform UBO {
	mat4 projection;
	vec4 lightDir;
	vec4 cameraDir;
	vec3 camPos;
} ubo;

//pushed descriptor set
layout (set = 1, binding = 1) uniform sampler2D normalTexture;

const float M_PI = 3.141592653589793;

float square(float x){return x*x;}
float qube(float x){return x*x*x;}

vec3 encode_norm(vec3 decoded){
	return decoded*0.5+0.5;
}

vec3 getNormal(){
	vec3 tangentNormal = (texture(normalTexture, inUV).xyz * 2.0 - 1.0);
	// tangentNormal = vec3(0,0,1);

	vec3 q1 = dFdx(inWorldPos);
	vec3 q2 = dFdy(inWorldPos);
	vec2 st1 = dFdx(inUV);
	vec2 st2 = dFdy(inUV);

	vec3 N = normalize(inNormal);
	// vec3 N = normalize(cross(q1,q2));
	vec3 T = normalize(q1 * st2.t - q2 * st1.t);
	vec3 B = -normalize(cross(N, T));
	mat3 TBN = mat3(T, B, N);

	return normalize(TBN * tangentNormal);
}

void main(){
	vec2 uv = inUV;

    vec3 normal = getNormal(); 

	outNormal = encode_norm(normal);
}