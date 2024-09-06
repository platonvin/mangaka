#version 450

layout(set = 0, binding = 0) uniform sampler2D depth_buffer;

layout (location = 0) in vec3 inWorldPos;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inNormal;
// layout (location = 3) in float depth;


// Scene bindings
layout (set = 0, binding = 1) uniform UBO {
	mat4 projection;
	vec4 lightDir;
	vec4 cameraDir;
	vec3 camPos;
} ubo;

#define MOD3 vec3(443.8975,397.2973, 491.1871)
float ss_rand(){
	vec2 p = gl_FragCoord.xy;
	vec3 p3  = fract(vec3(p.xyx) * MOD3);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

// #define MAX_NUM_JOINTS 128
layout (set = 1, binding = 0) uniform _UBO {
	// mat4 matrix;
	// mat4 jointMatrix[MAX_NUM_JOINTS];
	// uint jointCount;
	float v[];
} _ubo;

//pushed descriptor set
layout (set = 1, binding = 1) uniform sampler2D normalTexture;
layout (set = 1, binding = 2) uniform sampler2D metalRoughTexture;
layout (set = 1, binding = 3) uniform sampler2D albedoTexture;

// layout (push_constant) uniform PushConstants {
// 	int materialIndex;
// } pushConstants;

layout (location = 0) out vec4 outColor;

const float M_PI = 3.141592653589793;

float square(float x){return x*x;}
float qube(float x){return x*x*x;}

float ss_point_dither_1(){
	vec2 fpixel = gl_FragCoord.xy;

	//in pixels!
	float cell_size = 6.0;
	float radius = 2.8;
	//find nearest chessboard cell
	// ivec2 ipixel = ivec2(fpixel);
	vec2 nearest_cell = round(fpixel / cell_size);
	vec2 nearest_cell_center = nearest_cell*cell_size;// - cell_size/2.0;
	// vec2 nearest_cell_center = nearest_cell*cell_size;
	if(mod((nearest_cell.x + nearest_cell.y), 2) == 1){
		// return 1;
		//shift center towards another cell. But at which direction?
		vec2 direction = normalize(fpixel - nearest_cell_center);
		vec2 new_fpix = fpixel + direction * cell_size;
		//re-init
			nearest_cell = round(new_fpix / cell_size);
			nearest_cell_center = nearest_cell*cell_size;
			fpixel = new_fpix;
	}		

	if (distance(nearest_cell_center, fpixel) < radius) {
		return 0;
	} else {
		return 1;
	}
}
float ss_point_dither_2(){
	vec2 fpixel = gl_FragCoord.xy;

	//in pixels!
	float cell_size = 6.0;
	float radius = 2.8;
	//find nearest chessboard cell
	// ivec2 ipixel = ivec2(fpixel);
	vec2 nearest_cell = round(fpixel / cell_size);
	vec2 nearest_cell_center = nearest_cell*cell_size;// - cell_size/2.0;

	if (distance(nearest_cell_center, fpixel) < radius) {
		return 0;
	} else {
		return 1;
	}
}

float getMaxDepthDiff_simple(){
	ivec2 initial_pix = ivec2(gl_FragCoord.xy);

	float base_depth = texelFetch(depth_buffer, initial_pix, 0).x;
	float diff = 0;
	
	const int size=2;
	for(int xx=-size; xx<=+size; xx++){
	for(int yy=-size; yy<=+size; yy++){
		ivec2 pix = initial_pix + ivec2(xx,yy);
		float depth = texelFetch(depth_buffer, pix, 0).x;
		diff = max(diff, depth-base_depth);
	}}

	return diff;
}

const mat3 sobel_y = mat3(
	vec3(1.0, 0.0, -1.0),
	vec3(2.0, 0.0, -2.0),
	vec3(1.0, 0.0, -1.0)
);
const mat3 sobel_x = mat3(
	vec3(1.0, 2.0, 1.0),
	vec3(0.0, 0.0, 0.0),
	vec3(-1.0, -2.0, -1.0)
);
float getMaxDepthDiff_sobel(){
	ivec2 initial_pix = ivec2(gl_FragCoord.xy);
	ivec2 offset = ivec2(1);

	float depth = texelFetch(depth_buffer, initial_pix, 0).x;

	float n = texelFetch(depth_buffer, initial_pix + ivec2(0.0, -offset.y), 0).x;
	float s = texelFetch(depth_buffer, initial_pix + ivec2(0.0, offset.y), 0).x;
	float e = texelFetch(depth_buffer, initial_pix + ivec2(offset.x, 0.0), 0).x;
	float w = texelFetch(depth_buffer, initial_pix + ivec2(-offset.x, 0.0), 0).x;
	float nw = texelFetch(depth_buffer, initial_pix + ivec2(-offset.x, -offset.y), 0).x;
	float ne = texelFetch(depth_buffer, initial_pix + ivec2(offset.x, -offset.y), 0).x;
	float sw = texelFetch(depth_buffer, initial_pix + ivec2(-offset.x, offset.y), 0).x;
	float se = texelFetch(depth_buffer, initial_pix + ivec2(offset.x, offset.y), 0).x;

	mat3 surrounding_pixels = mat3(
		vec3(nw, n, ne),
		vec3(w, depth, e),
		vec3(sw, s, se)
	);

	float edge_x = dot(sobel_x[0], surrounding_pixels[0]) + dot(sobel_x[1], surrounding_pixels[1]) + dot(sobel_x[2], surrounding_pixels[2]);
	float edge_y = dot(sobel_y[0], surrounding_pixels[0]) + dot(sobel_y[1], surrounding_pixels[1]) + dot(sobel_y[2], surrounding_pixels[2]);

	float edge = sqrt(pow(edge_x, 2.0)+pow(edge_y, 2.0));

	// float edge_threshold = 0.1;
	return edge;
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
    float albedo = texture(albedoTexture, uv).x;
    float rough = texture(metalRoughTexture, uv).y;
    // vec3 normal = (texture(normalTexture, uv).xyz); 	
    vec3 normal = getNormal(); 

	float color = albedo;

	{
		float color_low  = floor(color*12.0)/12.0;
		float color_high =  ceil(color*12.0)/12.0;
		int color_index = int(color*12.0);
		
		float diff = color - color_low;
		float weighted_diff = square(diff);

		// if(weighted_diff > ss_rand()){
		if(color_index == 7){
			color = ss_point_dither_2()/1.5 + 0.2;
		}
		// } else {
		// 	color = color_low;

		//[-0.5 ~ +0.5]
		// float diff = q_color - color;
		// float remapping = 2.0 * sign(diff)*square(abs(diff));
		// color = q_color + remapping;
	}{

	}{
		float sided = abs(dot(normal, ubo.cameraDir.xyz));
		float outline_norm = smoothstep(.15,.25,sided);
		// if (sided < 0.5) color = 0;
		color *= outline_norm;

		vec3 tangentNormal = (texture(normalTexture, inUV).xyz * 2.0 - 1.0);

		color *= smoothstep(0.8,0.12, length(dFdx(tangentNormal)));
		color *= smoothstep(0.8,0.12, length(dFdy(tangentNormal)));

		// color = -dot(normal, ubo.cameraDir.xyz);
	}{
		// float depth_diff = getMaxDepthDiff_simple();
		float depth_diff = getMaxDepthDiff_sobel();
		// if(depth_diff*1000.0 > 0.2) color = 0;
		// if(depth_diff*4000.0 > 0.2){
		// 	color = 0;
		// }
		// float outline_depth = smoothstep(.25,.27, depth_diff*1000.0);
		float outline_depth = smoothstep(.19,.28, depth_diff*1000.0);
		color *= 1-outline_depth;
		
	}
	outColor = vec4(vec3(color), 1.0);
	// outColor = vec4(albedo, 1.0);
	// outColor = vec4(normal, 1.0);
	// outColor = vec4(abs(inWorldPos), 1.0);
	// outColor = vec4(1);
}