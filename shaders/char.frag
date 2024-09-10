#version 450

#extension GL_GOOGLE_include_directive : require
#include "blue_noise.glsl"

layout(set = 0, binding = 1) uniform sampler2D depth_buffer;
layout(set = 0, binding = 2) uniform sampler2D norm_buffer;
// layout(set = 0, binding = 3) uniform sampler2DShadow lightmap;
layout(set = 0, binding = 3) uniform sampler2D lightmap;

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inLocalPos;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inNormal;
// layout (location = 3) in float depth;


const uint 	SHADING_TYPE_FLAT_COLOR = 1 << 0;
const uint SHADING_TYPE_DOTS       = 1 << 1;
const uint SHADING_TYPE_HATCHES    = 1 << 2;
const uint SHADING_TYPE_BLUE_NOISE = 1 << 3;
const uint SHADES_COUNT = 10;
// const uint MATERIALS[SHADES_COUNT] = {
// 	SHADING_TYPE_FLAT_COLOR,
// };

layout (set = 0, binding = 0) uniform UBO {
	mat4 projection;
	mat4 lprojection;
	vec4 lightDir;
	vec4 cameraDir;
	vec3 camPos;
} ubo;

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

float hash13(vec3 p3){
	p3  = fract(p3 * .1031);
    p3 += dot(p3, p3.zyx + 31.32);
    return fract((p3.x + p3.y) * p3.z);
}
#define MOD3 vec3(443.8975,397.2973, 491.1871)
float ss_rand(){
	vec2 p = gl_FragCoord.xy;
	vec3 p3  = fract(vec3(p.xyx) * MOD3);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

const float PI = 3.14159265;

const float M_PI = 3.141592653589793;

float square(float x){return x*x;}
float qube(float x){return x*x*x;}

float sample_lightmap_with_shift(int xx, int yy, vec2 base_uv, float test_depth){
    vec2 pcfshift = vec2(1.0/1024.0);
    vec2 lighmap_shift = vec2(xx, yy) * pcfshift;

    float shadow = float(texture(lightmap, vec2(base_uv + lighmap_shift)).r > test_depth); //TODO PCF
    return shadow;
}
vec2 clip2uv(vec2 clip){
	vec2 uv;
	uv = clip/2.0 + 0.5;
	return uv;
}

float sample_lightmap(vec3 world_pos, vec3 normal){
    float b = (float((dot(normal, ubo.lightDir.xyz) < 0.0))*2.0 - 1.0);

    vec3 biased_pos = world_pos;
    // float bias;
    // if(dot(normal, ubo.lightDir.xyz) > 0.0){
    //     biased_pos -= normal*.01;
    //     bias = -0.001;
    // } else {
    //     biased_pos += normal*.01;
    //     bias = +0.001;
    // }

    vec3 light_clip = (ubo.lprojection* vec4(biased_pos,1)).xyz; //move up
         light_clip.z = light_clip.z;
    float world_depth = light_clip.z;
    
    // float shadow = float(texture(lightmap, vec2(light_clip.xy)).r < world_depth); //TODO PCF
    // return (1.0-shadow);
    float shadow = texture(lightmap, vec2(clip2uv(light_clip.xy))).r; //TODO PCF
    // return float(shadow - world_depth);
    // return float(shadow > world_depth-0.001);
    // return float(shadow);
    // float bias = 0.0 * (float((dot(normal, ubo.globalLightDir.xyz) < 0.0))*2.0 - 1.0);

    vec2 light_uv = clip2uv(light_clip.xy);
    // float bias = 0.0;

    // const float PI = 3.15;
    // const int sample_count = 1; //in theory i should do smth with temporal accumulation 
    // const float max_radius = 2.0 / 1000.0;
    // float angle = 00;
    // float normalized_radius = 00;
    // float norm_radius_step = 1.0 / float(sample_count);

    float total_light = 00;
    // float total_weight = 00;
    // vec2 ratio = vec2(1);

    // vec2 pcfshift = vec2(1.0/1024.0);

    total_light += sample_lightmap_with_shift(-1,0,light_uv, world_depth-0.001);
    total_light += sample_lightmap_with_shift(0,0,light_uv,  world_depth-0.001);
    total_light += sample_lightmap_with_shift(1,0,light_uv,  world_depth-0.001);
    total_light += sample_lightmap_with_shift(0,-1,light_uv, world_depth-0.001);
    total_light += sample_lightmap_with_shift(0,1,light_uv,  world_depth-0.001);

    return float((total_light / 5.0) > 0);
}

// can be improved with https://petrelharp.github.io/circle_rectangle_intersection/circle_rectangle_intersection.html
float msPixCircle(vec2 fpixel, float pixel_size, vec2 nearest_cell_center, float radius, int samples){
	// int samples = 8;
	int inside_count = 0;

	for (int xx = 0; xx < samples; xx++) {
	for (int yy = 0; yy < samples; yy++) {
		vec2 sample_pix = fpixel + pixel_size * vec2(xx + 0.5, yy + 0.5) / float(samples);
		if (distance(sample_pix, nearest_cell_center) < radius) {
			inside_count++;
		}
	}
    }
	return float(inside_count) / float(samples * samples);
}
//how big points should be for same color on average
float get_dither_radius(float color, float cell_size){
	//if color is 0 radius is 0
	//if color is 1 radius is (surely) >cell_size

	//avg_color = PI*r^2 / cell_size^2
	//r = sqrt((avg_color * cell_size^2) / PI)
	float r = sqrt((color * square(cell_size)) / PI);
	return r;
}
float ss_point_dither_1(float cell_size, float radius){
	vec2 fpixel = gl_FragCoord.xy;

	//find nearest chessboard cell
	vec2 nearest_cell = round(fpixel / cell_size);
	vec2 nearest_cell_center = nearest_cell*cell_size;// - cell_size/2.0;
	// vec2 nearest_cell_center = nearest_cell*cell_size;
	if(mod((nearest_cell.x + nearest_cell.y), 2) == 1){
		// return 1;
		//shift center towards another cell. But at which direction?
		vec2 direction = normalize(fpixel - nearest_cell_center);
		vec2 new_fpix = fpixel + direction * cell_size / 8.0;
		//re-init
			nearest_cell = round(new_fpix / cell_size);
			nearest_cell_center = nearest_cell*cell_size;
			fpixel = new_fpix;
	}		
	return msPixCircle(fpixel, 1.0, nearest_cell_center, radius, 8);
}
float ss_point_dither_2(float cell_size, float radius){
	vec2 fpixel = gl_FragCoord.xy;

	//in pixels!
	//find nearest chessboard cell
	// ivec2 ipixel = ivec2(fpixel);
	vec2 nearest_cell = round(fpixel / cell_size);
	vec2 nearest_cell_center = nearest_cell*cell_size;// - cell_size/2.0;

	return msPixCircle(fpixel, 1.0, nearest_cell_center, radius, 8);
	// if (distance(nearest_cell_center, fpixel) < radius) {
	// 	return 0;
	// } else {
	// 	return 1;
	// }
}

const int HATCH_DIRECTIONS = 7;
vec3 hatch_directions[HATCH_DIRECTIONS] = {
        normalize(vec3(-0.64, -0.06, 0.76)),
        normalize(vec3(0.70, 0.68, -0.21)),
        normalize(vec3(0.61, -0.38, 0.70)),
        normalize(vec3(-0.12, 0.87, -0.48)),
        normalize(vec3(0.74, 0.23, -0.63)),
        normalize(vec3(-0.76, -0.65, 0.03)),
        normalize(vec3(-0.03, -0.63, -0.78)),
};
float _pow (float base, float power){
	 return exp(power*log(base));
}
//how wide lines should be for same color on average
float get_hatch_dist(float color, float width){
	//single hatch pass covers:
	//(width)/(width+dist) of uncovered
	//so after n'th hatch there is In white space left
	//I0 = 1 = (d/(w+d))^0;
	//I1 = 1 * (d/(w+d))^1 = (d/(w+d))^1;
	//I2 = (1 * (d/(w+d))^1) * (d/(w+d))^1 = (d/(w+d))^2;
	//In = (d/(w+d))^n
	//avg_color = (dist/(width+dist))^(HATCH_COUNT)
	//d/(w+d) = color^(1/HC)
	//d = (color^(1/HC)) * (w+d)
	//d * (1 - color^(1/HC)) = w * color^(1/HC)
	float pw = _pow(color, 1.0/float(HATCH_DIRECTIONS));
	float dist = (width * pw) / (1 - pw);

	return dist;
}
float  ws_hatches_1(vec3 wpos, float width, float dist){
	float individual_width = width / float(HATCH_DIRECTIONS);
	int total_paper = 0;
	
	for(int i=0; i<HATCH_DIRECTIONS; i++){
		vec3 direction = hatch_directions[i];
		float modf = mod(dot(wpos, direction), individual_width+dist);
		
		if(modf <= individual_width) {
			//we are now inside hatch line
			//but hatches are not lines, they are not smooth
			vec3 rnd_driver = vec3(gl_FragCoord.xy, 1.0);
			float rnd = hash13(rnd_driver);

			// normalized value of how pixel is close to edge between hatch and non-hatch. Closer to edge, less black pixels 
			float dist_to_edge = (min(abs(modf - individual_width), modf) / individual_width)*2.0;
			// if(rnd > dist_to_edge){
				// return 1;
			// }
			// else{
			total_paper++;
			// }
			return 0;
		}
	}
	
	return 1;
	// return (1 - float(total_paper) / float(HATCH_DIRECTIONS));
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

vec3 decode_norm(vec3 encoded){
	return normalize(encoded*2.0-1.0);
}

float getMaxNormDiff_simple(){
	ivec2 initial_pix = ivec2(gl_FragCoord.xy);

	vec3 base = decode_norm(texelFetch(norm_buffer, initial_pix, 0).xyz);
	vec3 diff = vec3(0);
	
	const int size=2;
	for(int xx=-size; xx<=+size; xx++){
	for(int yy=-size; yy<=+size; yy++){
		ivec2 pix = initial_pix + ivec2(xx,yy);
		vec3 norm = decode_norm(texelFetch(norm_buffer, pix, 0).xyz);
		diff = max(diff, abs(norm-base));
	}}

	return length(diff);
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
float getMaxDepthDiff_sobel(sampler2D samp){
	ivec2 initial_pix = ivec2(gl_FragCoord.xy);
	ivec2 offset = ivec2(1);

	float depth = texelFetch(depth_buffer, initial_pix, 0).x;

	float n = texelFetch(samp, initial_pix + ivec2(0.0, -offset.y), 0).x;
	float s = texelFetch(samp, initial_pix + ivec2(0.0, offset.y), 0).x;
	float e = texelFetch(samp, initial_pix + ivec2(offset.x, 0.0), 0).x;
	float w = texelFetch(samp, initial_pix + ivec2(-offset.x, 0.0), 0).x;
	float nw = texelFetch(samp, initial_pix + ivec2(-offset.x, -offset.y), 0).x;
	float ne = texelFetch(samp, initial_pix + ivec2(offset.x, -offset.y), 0).x;
	float sw = texelFetch(samp, initial_pix + ivec2(-offset.x, offset.y), 0).x;
	float se = texelFetch(samp, initial_pix + ivec2(offset.x, offset.y), 0).x;

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

float getMaxNormDiff_sobel(sampler2D samp){
	ivec2 initial_pix = ivec2(gl_FragCoord.xy);
	ivec2 offset = ivec2(1);

	vec3 normal = decode_norm(texelFetch(depth_buffer, initial_pix, 0).rgb);

	vec3 n = decode_norm(texelFetch(samp, initial_pix + ivec2(0.0, -offset.y), 0).rgb);
	vec3 s = decode_norm(texelFetch(samp, initial_pix + ivec2(0.0, offset.y), 0).rgb);
	vec3 e = decode_norm(texelFetch(samp, initial_pix + ivec2(offset.x, 0.0), 0).rgb);
	vec3 w = decode_norm(texelFetch(samp, initial_pix + ivec2(-offset.x, 0.0), 0).rgb);
	vec3 nw = decode_norm(texelFetch(samp, initial_pix + ivec2(-offset.x, -offset.y), 0).rgb);
	vec3 ne = decode_norm(texelFetch(samp, initial_pix + ivec2(offset.x, -offset.y), 0).rgb);
	vec3 sw = decode_norm(texelFetch(samp, initial_pix + ivec2(-offset.x, offset.y), 0).rgb);
	vec3 se = decode_norm(texelFetch(samp, initial_pix + ivec2(offset.x, offset.y), 0).rgb);

	mat3 surrounding_pixels = mat3(
		vec3(length(nw-normal), length(n-normal), length(ne-normal)),
		vec3(length(w-normal), length(normal-normal), length(e-normal)),
		vec3(length(sw-normal), length(s-normal), length(se-normal))
	);

	float edge_x = dot(sobel_x[0], surrounding_pixels[0]) + dot(sobel_x[1], surrounding_pixels[1]) + dot(sobel_x[2], surrounding_pixels[2]);
	float edge_y = dot(sobel_y[0], surrounding_pixels[0]) + dot(sobel_y[1], surrounding_pixels[1]) + dot(sobel_y[2], surrounding_pixels[2]);

	float edge = sqrt(pow(edge_x, 2.0)+pow(edge_y, 2.0));

	// float edge_threshold = 0.1;
	return edge;
}

vec3 calcNormal(){
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
vec3 loadNormal(){
    vec3 normal = normalize(texelFetch(norm_buffer, ivec2(gl_FragCoord.xy), 0).xyz * 2.0 - 1.0); 	
	return normal;
}

void main(){
	vec2 uv = inUV;
    float albedo = texture(albedoTexture, uv).x;
    float rough = texture(metalRoughTexture, uv).y;
	
    // vec3 normal = calcNormal(); 
    vec3 normal = loadNormal(); 

	float albedo_low  = floor(albedo*12.0)/12.0;
	float albedo_high =  ceil(albedo*12.0)/12.0;
	albedo = albedo_low;
	float color = albedo;

	{
	}{

	}{
		float norm_diff = getMaxNormDiff_sobel(norm_buffer);
		// float norm_diff = getMaxNormDiff_simple();
		// float outline_norm = smoothstep(.27, .35, norm_diff);
		float outline_norm = smoothstep(.35, .42, norm_diff);
		color *= 1-outline_norm;
		// if(norm_diff > 0.42) color = 0;
	}{
		float depth_diff = getMaxDepthDiff_simple();
		// float depth_diff = getMaxDepthDiff_sobel(depth_buffer);
		// if(depth_diff*1000.0 > 0.2) color = 0;
		// if(depth_diff*4000.0 > 0.2){
		// 	color = 0;
		// }
		// float outline_depth = smoothstep(.25,.27, depth_diff*1000.0);
		// float outline_depth = smoothstep(.18,.25, depth_diff*1000.0);
		float outline_depth = smoothstep(.25,.28, depth_diff*1.0);
		color *= 1-outline_depth;
	}
	{
		float lightmap_light = sample_lightmap(inWorldPos, normal);
		//if in sulight
		float light = lightmap_light;
		// if(light > 0){
		// 	light = -dot(normal, ubo.lightDir.xyz);
		// 	outColor = vec4(vec3(light),1);
		// 	return;
		// }
		light += 0.15; //ambient
		
		color *= light; //before that we were only calculating "albedo"
		

		if((color >= 0.05) && (color <= 0.15)){
			float width = 0.003;
			// color = ws_hatches_1(inWorldPos, width, get_hatch_dist(color, width));
			// color = ws_hatches_1(inLocalPos, width*4.0, get_hatch_dist(color, width));
			color = ws_hatches_1(inLocalPos, width*2.0, get_hatch_dist(color, width));
			// float blue_noise = hilbert_r1_blue_noisef(uvec2(gl_FragCoord));
			// color = (.5 > blue_noise) ? 1 : 0;
			// color = ws_hatches_1(inLocalPos, normalize(cross(normal, vec3(1,1,1))), width, get_hatch_dist(color, width));
			// color = ws_hatches_1(inLocalPos, get_hatch_dist(color, width), width);
		} else 
		// if((color > 0.35) && (color < 0.45)){
		if((color > 0.45) && (color < 0.66)){
			float blue_noise = hilbert_r1_blue_noisef(uvec2(gl_FragCoord));
			// color = (.5 > blue_noise) ? 1 : 0;
			color = blue_noise;

		}
		else if((color <= 0.95)){ 
			float color_low  = floor(color*6.0)/6.0;
			float color_high =  ceil(color*6.0)/6.0;
			color = color_low;			
			
			// float cell_size = 1.25/color;
			float cell_size = 5.0;// / (1.0+ color);
			color = ss_point_dither_2(cell_size, get_dither_radius(color, cell_size));
		}
	}{
		color = color*0.69 + 0.1;
	}

	vec3 color3 = vec3(color);
	outColor = vec4(color3, 1.0);
	// outColor = vec4(albedo, 1.0);
}