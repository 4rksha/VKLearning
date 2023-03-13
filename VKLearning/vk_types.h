// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#include <vulkan/vulkan.h>
#if _DEBUG
#define VMA_RECORDING_ENABLED 1
#endif
#include <vk_mem_alloc.h>

#include <deque>
#include <functional>
#include <glm/glm.hpp>

#define GPU_DATA __declspec(align(16))

struct AllocatedBuffer 
{
    VkBuffer      buffer;
    VmaAllocation allocation;
};

struct AllocatedImage 
{
    VkImage       image;
    VmaAllocation allocation;
};


struct DeletionQueue
{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()>&& function) {
		deletors.push_back(function);
	}

	void flush() {
		// reverse iterate the deletion queue to execute all the functions
		for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
			(*it)(); //call the function
		}

		deletors.clear();
	}
};

struct MeshPushConstants
{
	glm::mat4 renderMatrix;
};

struct Material
{
	VkDescriptorSet textureSet{ VK_NULL_HANDLE };
	VkPipeline pipeline{};
	VkPipelineLayout pipelineLayout{};
};


GPU_DATA struct GPUCameraData {
	glm::mat4 view{};
	glm::mat4 proj{};
	glm::mat4 viewproj{};
	glm::vec4 cameraPosition{};
};

GPU_DATA struct GPUSceneData {
	glm::vec3 lightDirection = glm::vec3(0.f); //w for sun power
	int lightNb = 1;
	glm::vec3 lightColor = glm::vec3(0.f);
	float metallic = 0.f; //x for min, y for max, zw unused.
	glm::vec3 albedo = glm::vec3(1.f, 0.f, 0.f); // w is for exponent
	float roughness = 0.f;
};

GPU_DATA struct GPULightData
{
	glm::vec3 color{1.f, 1.f, 1.f };
	float intensity{1.f};
	glm::vec3 position{1.f, 1.f, 1.f};
	
};


struct UploadContext {
	VkFence uploadFence;
	VkCommandPool commandPool;
	VkCommandBuffer commandBuffer;
};

struct FrameData {
	VkSemaphore presentSemaphore, renderSemaphore;
	VkFence renderFence;

	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;

	AllocatedBuffer cameraBuffer;
	VkDescriptorSet globalDescriptor;

	AllocatedBuffer objectBuffer;
	VkDescriptorSet objectDescriptor;

	AllocatedBuffer lightBuffer;
	VkDescriptorSet lightDescriptor;
};

struct GPUObjectData {
	glm::mat4 modelMatrix;
};

struct Texture
{
	AllocatedImage image;
	VkImageView imageView;
};
