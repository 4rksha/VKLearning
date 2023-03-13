// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "vk_types.h"
#include <vector>
#include <array>

#include "camera.h"

#include "vk_mesh.h"


constexpr uint32_t WIDTH = 1280;
constexpr uint32_t HEIGHT = 720;
constexpr float ASPECT_RATIO = static_cast<float>(WIDTH) / static_cast<float>(HEIGHT);

constexpr uint64_t FRAME_OVERLAP = 2;

class VulkanEngine
{
public:

	void init();
	void cleanup();
	void run();
	void draw();

	VkExtent2D	m_windowExtent{ WIDTH , HEIGHT };
	SDL_Window* m_window{ nullptr };

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
	void drawObjects(VkCommandBuffer cmd, const RenderObject* first, const size_t count);

	AllocatedBuffer createBuffer(const size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const;
	VkDeviceSize padUniformBufferSize(size_t originalSize) const;

	void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const;

	void loadImages();

	bool processInput(const SDL_Event* e);
	bool processKeyboard(const SDL_Event* e);
	void processMouse(const SDL_Event* e);

	float getMeanDeltaTime() const;
	const Camera& getCamera() const { return m_camera;}


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
	VkDescriptorSetLayout m_lightSetLayout;

	std::unordered_map<std::string, Material> m_materials;
	std::unordered_map<std::string, Mesh>     m_meshes;

	VkPhysicalDeviceProperties m_gpuProperties;

	GPUSceneData    m_sceneParameters;
	AllocatedBuffer m_sceneParameterBuffer;

	UploadContext m_uploadContext;

	VkDescriptorSetLayout					 m_bindlessTextureSetLayout;
	std::unordered_map<std::string, Texture> m_loadedTextures;

	bool m_isInitialized{ false };
	int  m_frameNumber{ 0 };

	Camera m_camera{};

	float deltaTime{ 0 };

	std::deque<float> lastDeltaTimes{};

	std::array<GPULightData, 1000> m_lightData;
};

