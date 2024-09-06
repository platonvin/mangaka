/**
 * Vulkan glTF model and texture loading class based on tinyglTF (https://github.com/syoyo/tinygltf)
 *
 * Copyright (C) 2018-2024 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <iostream>
#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include "../../lum-al/src/al.hpp"
#include "../../lum-al/src/defines/macros.hpp"
#include "vulkan/vulkan.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <gli/gli.hpp>
#include <glm/gtx/string_cast.hpp>

#include "basisu/transcoder/basisu_transcoder.h"
// #include "basisu.hpp"

// ERROR is already defined in wingdi.h and collides with a define in the Draco headers
#if defined(_WIN32) && defined(ERROR) && defined(TINYGLTF_ENABLE_DRACO) 
#undef ERROR
#pragma message ("ERROR constant already defined, undefining")
#endif

#define TINYGLTF_NO_STB_IMAGE_WRITE

#if defined(__ANDROID__)
#define TINYGLTF_ANDROID_LOAD_FROM_ASSETS
#include <android/asset_manager.h>
#endif

#include "tiny_gltf.h"

using glm::mat4;
using glm::vec3, glm::vec4;
using std::vector;

// Changing this value here also requires changing it in the vertex shader
#define MAX_NUM_JOINTS 128u

namespace vkglTF
{
	struct Node;

	struct BoundingBox {
		vec3 min;
		vec3 max;
		bool valid = false;
		BoundingBox();
		BoundingBox(vec3 min, vec3 max);
		BoundingBox getAABB(mat4 m);
	};

	struct TextureSampler {
		VkFilter magFilter;
		VkFilter minFilter;
		VkSamplerAddressMode addressModeU;
		VkSamplerAddressMode addressModeV;
		VkSamplerAddressMode addressModeW;
	};

	struct Texture {
		Renderer* renderer;
		Image image = {};

		VkSampler sampler;
		void updateDescriptor();
		void destroy();
		void fromglTfImage(tinygltf::Image& gltfimage, std::string path, TextureSampler textureSampler, Renderer* renderer);
	};

	struct Material {		
		enum AlphaMode{ ALPHAMODE_OPAQUE, ALPHAMODE_MASK, ALPHAMODE_BLEND };
		AlphaMode alphaMode = ALPHAMODE_OPAQUE;
		float alphaCutoff = 1.0f;
		float metallicFactor = 1.0f;
		float roughnessFactor = 1.0f;
		vec4 baseColorFactor = vec4(1.0f);
		vec4 emissiveFactor = vec4(0.0f);
		vkglTF::Texture *baseColorTexture;
		vkglTF::Texture *metallicRoughnessTexture;
		vkglTF::Texture *normalTexture;
		vkglTF::Texture *occlusionTexture;
		vkglTF::Texture *emissiveTexture;
		bool doubleSided = false;
		struct TexCoordSets {
			uint8_t baseColor = 0;
			uint8_t metallicRoughness = 0;
			uint8_t specularGlossiness = 0;
			uint8_t normal = 0;
			uint8_t occlusion = 0;
			uint8_t emissive = 0;
		} texCoordSets;
		struct Extension {
			vkglTF::Texture *specularGlossinessTexture;
			vkglTF::Texture *diffuseTexture;
			vec4 diffuseFactor = vec4(1.0f);
			vec3 specularFactor = vec3(0.0f);
		} extension;
		struct PbrWorkflows {
			bool metallicRoughness = true;
			bool specularGlossiness = false;
		} pbrWorkflows;
		VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
		int index = 0;
		bool unlit = false;
		float emissiveStrength = 1.0f;
	};

	struct Primitive {
		uint32_t firstIndex;
		uint32_t indexCount;
		uint32_t vertexCount;
		Material &material;
		bool hasIndices;
		BoundingBox bb;
		Primitive(uint32_t firstIndex, uint32_t indexCount, uint32_t vertexCount, Material& material);
		void setBoundingBox(vec3 min, vec3 max);
	};

	struct Mesh {
		Renderer *renderer;
		vector<Primitive*> primitives;
		BoundingBox bb;
		BoundingBox aabb;
		Buffer uniformBuffer = {};
		struct UniformBlock {
			mat4 matrix;
			mat4 jointMatrix[MAX_NUM_JOINTS]{};
			uint32_t jointcount{ 0 };
		} uniformBlock;
		Mesh(Renderer* _renderer, glm::mat4 matrix);
		~Mesh();
		void setBoundingBox(vec3 min, vec3 max);
	};

	struct Skin {
		std::string name;
		Node *skeletonRoot = nullptr;
		vector<mat4> inverseBindMatrices;
		vector<Node*> joints;
	};

	struct Node {
		Node *parent;
		uint32_t index;
		vector<Node*> children;
		mat4 matrix;
		std::string name;
		Mesh *mesh;
		Skin *skin;
		int32_t skinIndex = -1;
		vec3 translation{};
		vec3 scale{ 1.0f };
		quat rotation{};
		BoundingBox bvh;
		BoundingBox aabb;
		bool useCachedMatrix{ false };
		mat4 cachedLocalMatrix{ mat4(1.0f) };
		mat4 cachedMatrix{ mat4(1.0f) };
		mat4 localMatrix();
		mat4 getMatrix();
		void update();
		~Node();
	};

	struct AnimationChannel {
		enum PathType { TRANSLATION, ROTATION, SCALE };
		PathType path;
		Node *node;
		uint32_t samplerIndex;
	};

	struct AnimationSampler {
		enum InterpolationType { LINEAR, STEP, CUBICSPLINE };
		InterpolationType interpolation;
		vector<float> inputs;
		vector<vec4> outputsVec4;
		vector<float> outputs;
		vec4 cubicSplineInterpolation(size_t index, float time, uint32_t stride);
		void translate(size_t index, float time, vkglTF::Node* node);
		void scale(size_t index, float time, vkglTF::Node* node);
		void rotate(size_t index, float time, vkglTF::Node* node);
	};

	struct Animation {
		std::string name;
		vector<AnimationSampler> samplers;
		vector<AnimationChannel> channels;
		float start = std::numeric_limits<float>::max();
		float end = std::numeric_limits<float>::min();
	};

	struct Model {

		// vks::VulkanDevice *device;
		Renderer* renderer;		

		struct Vertex {
			vec3 pos;
			vec3 normal;
			vec2 uv0;
			vec2 uv1;
			uvec4 joint0;
			vec4 weight0;
			vec4 color;
		};

		Buffer vertices;
		Buffer indices;

		mat4 aabb;

		vector<Node*> nodes;
		vector<Node*> linearNodes;

		vector<Skin*> skins;

		vector<Texture> textures;
		vector<TextureSampler> textureSamplers;
		vector<Material> materials;
		vector<Animation> animations;
		vector<std::string> extensions;

		struct Dimensions {
			vec3 min = vec3(FLT_MAX);
			vec3 max = vec3(-FLT_MAX);
		} dimensions;

		struct LoaderInfo {
			uint32_t* indexBuffer;
			Vertex* vertexBuffer;
			size_t indexPos = 0;
			size_t vertexPos = 0;
		};

		std::string filePath;

		void destroy();
		void loadNode(vkglTF::Node* parent, const tinygltf::Node& node, uint32_t nodeIndex, const tinygltf::Model& model, LoaderInfo& loaderInfo, float globalscale);
		void getNodeProps(const tinygltf::Node& node, const tinygltf::Model& model, size_t& vertexCount, size_t& indexCount);
		void loadSkins(tinygltf::Model& gltfModel);
		void loadTextures(tinygltf::Model& gltfModel, Renderer* _renderer);
		VkSamplerAddressMode getVkWrapMode(int32_t wrapMode);
		VkFilter getVkFilterMode(int32_t filterMode);
		void loadTextureSamplers(tinygltf::Model& gltfModel);
		void loadMaterials(tinygltf::Model& gltfModel);
		void loadAnimations(tinygltf::Model& gltfModel);
		void loadFromFile(std::string filename, Renderer* _renderer, float scale);
		void drawNode(Node* node, VkCommandBuffer commandBuffer);
		void draw(VkCommandBuffer commandBuffer);
		void calculateBoundingBox(Node* node, Node* parent);
		void getSceneDimensions();
		void updateAnimation(uint32_t index, float time);
		Node* findNode(Node* parent, uint32_t index);
		Node* nodeFromIndex(uint32_t index);
	};
}
