#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV0;
layout (location = 3) in vec2 inUV1;
layout (location = 4) in uvec4 inJoint0;
layout (location = 5) in vec4 inWeight0;
layout (location = 6) in vec4 inColor0;

layout (set = 0, binding = 0) uniform UBO{
	mat4 projection;
	vec4 lightDir;
	vec4 cameraDir;
	vec3 camPos;
} ubo;

#define MAX_NUM_JOINTS 128

layout (set = 1, binding = 0) uniform UBONode {
	mat4 matrix;
	mat4 jointMatrix[MAX_NUM_JOINTS];
	uint jointCount;
} node;

layout (push_constant) uniform PCO {
	mat4 model;
} pco;

void main() 
{
	vec4 locPos;
	if (node.jointCount > 0) {
		// Mesh is skinned
		mat4 skinMat = 
			inWeight0.x * node.jointMatrix[inJoint0.x] +
			inWeight0.y * node.jointMatrix[inJoint0.y] +
			inWeight0.z * node.jointMatrix[inJoint0.z] +
			inWeight0.w * node.jointMatrix[inJoint0.w];

		locPos = pco.model * node.matrix * skinMat * vec4(inPos, 1.0);
		// outNormal = normalize(transpose(inverse(mat3(pco.model * node.matrix * skinMat))) * inNormal);
	} else {
		locPos = pco.model * node.matrix * vec4(inPos, 1.0);
		// outNormal = normalize(transpose(inverse(mat3(pco.model * node.matrix))) * inNormal);
	}
	locPos.y = -locPos.y;
	vec3 outWorldPos = locPos.xyz / locPos.w;
	// outUV = inUV0;
	// outUV = inUV1;
	vec4 clip =  (ubo.projection * vec4(outWorldPos, 1.0));// * 1000000.0;
	// gl_Position.xy ;
	// clip.z /= 1000.0;
	// clip.z = clamp(clip.z,0,1);
	gl_Position = clip;
}