// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"
#include <vector>
#include <functional>
#include <deque>
#include <unordered_map>

#include "camera.h"

#include "vk_mem_alloc.h"
#include "vk_mesh.h"

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

struct RenderObject
{
	Mesh* mesh = nullptr;
	Material* material = nullptr;
	glm::mat4 transformMatrix{};
};

struct GPUCameraData {
	glm::mat4 view{};
	glm::mat4 proj{};
	glm::mat4 viewproj{};
};

struct GPUSceneData {
	glm::vec4 fogColor; // w is for exponent
	glm::vec4 fogDistances; //x for min, y for max, zw unused.
	glm::vec4 ambientColor;
	glm::vec4 sunlightDirection; //w for sun power
	glm::vec4 sunlightColor;
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
};

struct GPUObjectData {
	glm::mat4 modelMatrix;
};

struct Texture 
{
	AllocatedImage image;
	VkImageView imageView;
};

constexpr uint64_t FRAME_OVERLAP = 2;

class VulkanEngine
{
public:

	void init();
	void cleanup();
	void run();
	void draw();

	VkExtent2D		   m_windowExtent{ 1920 , 1080 };
	struct SDL_Window* m_window{ nullptr };

public:
	void initVulkan();
	void initSwapchain();
	void initCommands();

	void initDefaultRenderpass();
	void initFramebuffers();
	void initSyncStructures();
	void initDescriptors();
	void initPipelines();

	void initScene();

	bool loadShaderModule(const char* filePath, VkShaderModule* outShaderModule) const;

	void loadMeshes();
	void uploadMesh(Mesh& mesh);

	FrameData& getCurrentFrame();

	Material* createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name);
	Material* getMaterial(const std::string& name);
	Mesh* getMesh(const std::string& name);
	void drawObjects(VkCommandBuffer cmd, RenderObject* first, const size_t count);

	[[nodiscard]] AllocatedBuffer createBuffer(const size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;

	[[nodiscard]] VkDeviceSize padUniformBufferSize(size_t originalSize) const;

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;

	void loadImages();


public:

	VkInstance					m_instance;
	VkDebugUtilsMessengerEXT	m_debug_messenger;
	VkPhysicalDevice			m_chosenGPU;
	VkDevice					m_device;
	VkSurfaceKHR				m_surface;

	VkSwapchainKHR			 m_swapchain;
	VkFormat				 m_swapchainImageFormat;
	std::vector<VkImage>	 m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;

	VkRenderPass			   m_renderPass;
	std::vector<VkFramebuffer> m_framebuffers;

	VkQueue  m_graphicsQueue;
	uint32_t m_graphicsQueueFamily;

	FrameData m_frames[FRAME_OVERLAP];

	DeletionQueue m_mainDeletionQueue;

	VmaAllocator m_allocator;

	VkImageView    m_depthImageView;
	VkFormat	   m_depthFormat;
	AllocatedImage m_depthImage;

	std::vector<RenderObject> m_renderables;

	VkDescriptorPool	  m_descriptorPool;
	VkDescriptorSetLayout m_globalSetLayout;
	VkDescriptorSetLayout m_objectSetLayout;

	std::unordered_map<std::string, Material> m_materials;
	std::unordered_map<std::string, Mesh>     m_meshes;

	VkPhysicalDeviceProperties m_gpuProperties;

	GPUSceneData    m_sceneParameters;
	AllocatedBuffer m_sceneParameterBuffer;

	UploadContext m_uploadContext;

	VkDescriptorSetLayout					 m_singleTextureSetLayout;
	std::unordered_map<std::string, Texture> m_loadedTextures;

	bool m_isInitialized{ false };
	int  m_frameNumber{ 0 };

	Camera m_camera{};

	double m_fps{ 0 };
};

