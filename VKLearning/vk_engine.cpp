
#define VMA_IMPLEMENTATION

#define NOMINMAX 

#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include "vk_types.h"
#include "vk_initializers.h"

#include "VkBootstrap.h"

#include "vk_utils.h"
#include <iostream>
#include <fstream>

#include "vk_pipeline.h"
#include "glm/gtx/transform.hpp"

#include <chrono>

#include "vk_textures.h"

#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl.h>

#include "vk_ui.h"



constexpr unsigned int TIMEOUT = 1000000000;
constexpr unsigned int MAX_OBJECTS = 20000;

void VulkanEngine::init()
{
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	constexpr SDL_WindowFlags window_flags = SDL_WINDOW_VULKAN;

	m_window = SDL_CreateWindow(
		"Vulkan Renderer",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		static_cast<int>(m_windowExtent.width),
		static_cast<int>(m_windowExtent.height),
		window_flags
	);

	initVulkan();
	initSwapchain();
	initDefaultRenderpass();
	initFramebuffers();
	initCommands();
	initSyncStructures();
	VulkanUI::init(this);

	initDescriptors();
	initPipelines();
	loadImages();
	loadMeshes();

	initScene();

	//everything went fine
	m_isInitialized = true;
}

void VulkanEngine::cleanup()
{
	if (m_isInitialized)
	{
		vkDeviceWaitIdle(m_device);

		m_mainDeletionQueue.flush();
		vmaDestroyAllocator(m_allocator);

		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

		vkb::destroy_debug_utils_messenger(m_instance, m_debug_messenger);

		vkDestroyDevice(m_device, nullptr);
		vkDestroyInstance(m_instance, nullptr);

		SDL_DestroyWindow(m_window);
	}
}

void VulkanEngine::run()
{
	SDL_Event e;
	bool bQuit = false;

	const auto start = std::chrono::system_clock::now();
	std::chrono::duration<float> elapsed_seconds = std::chrono::system_clock::now() - start;

	//main loop
	while (!bQuit)
	{
		std::chrono::duration<float> prev_elapsed_seconds = elapsed_seconds;
		elapsed_seconds = std::chrono::system_clock::now() - start;
		float deltaTime = (elapsed_seconds - prev_elapsed_seconds).count();


		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			ImGui_ImplSDL2_ProcessEvent(&e);
			m_camera.processInputEvent(&e);

			if (e.type == SDL_QUIT || (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE))
				bQuit = true;
		}

		VulkanUI::updateImGui(this);
		m_camera.updateCamera(deltaTime * 1000.f);

		draw();
	}
}

void VulkanEngine::draw()
{
	ImGui::Render();
	VK_CHECK(vkWaitForFences(m_device, 1, &getCurrentFrame().renderFence, true, TIMEOUT));
	VK_CHECK(vkResetFences(m_device, 1, &getCurrentFrame().renderFence));
	const auto start = std::chrono::high_resolution_clock::now();

	uint32_t swapchainImageIndex;
	VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, TIMEOUT, getCurrentFrame().presentSemaphore, VK_NULL_HANDLE, &swapchainImageIndex));

	VK_CHECK(vkResetCommandBuffer(getCurrentFrame().mainCommandBuffer, 0));

	VkCommandBuffer cmd = getCurrentFrame().mainCommandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let Vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo{};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;

	cmdBeginInfo.pInheritanceInfo = nullptr;
	cmdBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	VkClearValue clearValue{};
	clearValue.color = { { 0.0f, 0.0f, 0.0f, 1.0f } };

	VkClearValue depthClear;
	depthClear.depthStencil.depth = 1.f;

	//start the main renderpass.
	VkRenderPassBeginInfo rpInfo = vkinit::renderpassBeginInfo(m_renderPass, m_windowExtent, m_framebuffers[swapchainImageIndex]);

	//connect clear values
	rpInfo.clearValueCount = 2;
	const VkClearValue clearValues[] = { clearValue, depthClear };
	rpInfo.pClearValues = &clearValues[0];

	vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

	drawObjects(cmd, m_renderables.data(), m_renderables.size());
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
	vkCmdEndRenderPass(cmd);
	const auto end = std::chrono::high_resolution_clock::now();

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue.
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished

	VkSubmitInfo submit{};
	submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit.pNext = nullptr;

	constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	submit.pWaitDstStageMask = &waitStage;

	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &getCurrentFrame().presentSemaphore;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &getCurrentFrame().renderSemaphore;

	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &cmd;

	//submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submit, getCurrentFrame().renderFence));

	// this will put the image we just rendered into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as it's necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;

	presentInfo.pSwapchains = &m_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &getCurrentFrame().renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));

	//increase the number of frames drawn
	m_frameNumber++;
	m_fps = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) / 60.;
}

void VulkanEngine::initVulkan()
{
	vkb::InstanceBuilder builder;

	vkb::detail::Result instRet = builder.set_app_name("Vulkan Renderer")
		.request_validation_layers(true)
		.require_api_version(1, 2, 0)
		.use_default_debug_messenger()
		.build();

	vkb::Instance vkb_inst = instRet.value();

	m_instance = vkb_inst.instance;
	m_debug_messenger = vkb_inst.debug_messenger;

	// get the surface of the window we opened with SDL
	SDL_Vulkan_CreateSurface(m_window, m_instance, &m_surface);

	//use vkbootstrap to select a GPU.
	//We want a GPU that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector selector{ vkb_inst };
	vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 2)
		.set_surface(m_surface)
		.select()
		.value();

	//create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{ physicalDevice };

	VkPhysicalDeviceVulkan11Features featuresInfo = {};
	featuresInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
	featuresInfo.shaderDrawParameters = VK_TRUE;

	vkb::Device vkbDevice = deviceBuilder.add_pNext(&featuresInfo).build().value();
	m_gpuProperties = vkbDevice.physical_device.properties;

	// Get the VkDevice handle used in the rest of a Vulkan application
	m_device = vkbDevice.device;
	m_chosenGPU = physicalDevice.physical_device;

	m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();


	VmaAllocatorCreateInfo allocatorInfo = {};
	allocatorInfo.physicalDevice = m_chosenGPU;
	allocatorInfo.device = m_device;
	allocatorInfo.instance = m_instance;
#if VMA_RECORDING_ENABLED
	VmaRecordSettings recordSettingsInfo = {};
	recordSettingsInfo.pFilePath = "../../log.txt";
	allocatorInfo.pRecordSettings = &recordSettingsInfo;
#endif
	vmaCreateAllocator(&allocatorInfo, &m_allocator);
}

void VulkanEngine::initSwapchain()
{
	vkb::SwapchainBuilder swapchainBuilder{ m_chosenGPU, m_device, m_surface };

	vkb::Swapchain vkbSwapchain = swapchainBuilder
		.use_default_format_selection()
		//use vsync present mode
		.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
		.set_desired_extent(m_windowExtent.width, m_windowExtent.height)
		.build()
		.value();

	//store swapchain and its related images
	m_swapchain = vkbSwapchain.swapchain;
	m_swapchainImages = vkbSwapchain.get_images().value();
	m_swapchainImageViews = vkbSwapchain.get_image_views().value();

	m_swapchainImageFormat = vkbSwapchain.image_format;


	VkExtent3D depthImageExtent = {
		m_windowExtent.width,
		m_windowExtent.height,
		1
	};

	//hardcoding the depth format to 32 bit float
	m_depthFormat = VK_FORMAT_D32_SFLOAT;

	//the depth image will be an image with the format we selected and Depth Attachment usage flag
	VkImageCreateInfo dimgInfo = vkinit::imageCreateInfo(m_depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depthImageExtent);

	//for the depth image, we want to allocate it from GPU local memory
	VmaAllocationCreateInfo dimgAllocinfo = {};
	dimgAllocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	dimgAllocinfo.requiredFlags = static_cast<VkMemoryPropertyFlags>(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	//allocate and create the image
	vmaCreateImage(m_allocator, &dimgInfo, &dimgAllocinfo, &m_depthImage.image, &m_depthImage.allocation, nullptr);

	//build an image-view for the depth image to use for rendering
	VkImageViewCreateInfo dviewInfo = vkinit::imageviewCreateInfo(m_depthFormat, m_depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

	VK_CHECK(vkCreateImageView(m_device, &dviewInfo, nullptr, &m_depthImageView));

	//add to deletion queues
	m_mainDeletionQueue.push_function([=, this]() {

		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

		vkDestroyImageView(m_device, m_depthImageView, nullptr);
		vmaDestroyImage(m_allocator, m_depthImage.image, m_depthImage.allocation);
		});
}

void VulkanEngine::initCommands()
{
	const VkCommandPoolCreateInfo commandPoolInfo = vkinit::commandPoolCreateInfo(m_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

	for (auto& m_frame : m_frames)
	{
		VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_frame.commandPool));

		VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::commandBufferAllocateInfo(m_frame.commandPool, 1);
		VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_frame.mainCommandBuffer));
	}

	const VkCommandPoolCreateInfo uploadCommandPoolInfo = vkinit::commandPoolCreateInfo(m_graphicsQueueFamily);
	//create pool for upload context
	VK_CHECK(vkCreateCommandPool(m_device, &uploadCommandPoolInfo, nullptr, &m_uploadContext.commandPool));

	//allocate the default command buffer that we will use for the instant commands
	const VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::commandBufferAllocateInfo(m_uploadContext.commandPool, 1);

	VK_CHECK(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &m_uploadContext.commandBuffer));


	m_mainDeletionQueue.push_function([=, this]() {
		for (const auto& m_frame : m_frames)
		{
			vkDestroyCommandPool(m_device, m_frame.commandPool, nullptr);
		}
		vkDestroyCommandPool(m_device, m_uploadContext.commandPool, nullptr);
		});
}

void VulkanEngine::initDefaultRenderpass()
{
	VkAttachmentDescription colorAttachement = {};
	colorAttachement.format = m_swapchainImageFormat;
	colorAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachement.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachementRef{};
	colorAttachementRef.attachment = 0;
	colorAttachementRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachement{};
	depthAttachement.flags = 0;
	depthAttachement.format = m_depthFormat;
	depthAttachement.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachement.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachement.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachement.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachement.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachement.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachement.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachementRef{};
	depthAttachementRef.attachment = 1;
	depthAttachementRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachementRef;
	subpass.pDepthStencilAttachment = &depthAttachementRef;

	const VkAttachmentDescription attachments[2] = { colorAttachement, depthAttachement };

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;


	//connect the color attachment to the info
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = &attachments[0];
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;


	VkSubpassDependency dependencies[2] = {};
	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = 0;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	dependencies[1].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].dstSubpass = 0;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].srcAccessMask = 0;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	renderPassInfo.dependencyCount = 2;
	renderPassInfo.pDependencies = &dependencies[0];

	VK_CHECK(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass));
	m_mainDeletionQueue.push_function([=, this]()
		{
			vkDestroyRenderPass(m_device, m_renderPass, nullptr);
		});
}

void VulkanEngine::initFramebuffers()
{
	//create the framebuffers for the swapchain images. This will connect the render-pass to the images for rendering
	VkFramebufferCreateInfo fbInfo = {};
	fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fbInfo.pNext = nullptr;

	fbInfo.renderPass = m_renderPass;
	fbInfo.attachmentCount = 2;
	fbInfo.width = m_windowExtent.width;
	fbInfo.height = m_windowExtent.height;
	fbInfo.layers = 1;

	m_framebuffers = std::vector<VkFramebuffer>(m_swapchainImages.size());

	for (unsigned i = 0; i < m_swapchainImages.size(); i++)
	{
		VkImageView attachments[2];
		attachments[0] = m_swapchainImageViews[i];
		attachments[1] = m_depthImageView;
		fbInfo.pAttachments = attachments;
		VK_CHECK(vkCreateFramebuffer(m_device, &fbInfo, nullptr, &m_framebuffers[i]));
	}
	m_mainDeletionQueue.push_function([=, this]()
		{
			for (unsigned i = 0; i < m_swapchainImages.size(); i++) {
				vkDestroyFramebuffer(m_device, m_framebuffers[i], nullptr);
				vkDestroyImageView(m_device, m_swapchainImageViews[i], nullptr);
			}
		});

}

void VulkanEngine::initSyncStructures()
{
	const VkFenceCreateInfo fenceCreateInfo = vkinit::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);
	const VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphoreCreateInfo();

	for (auto& m_frame : m_frames)
	{
		VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_frame.renderFence));

		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frame.presentSemaphore));
		VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_frame.renderSemaphore));
	}

	const VkFenceCreateInfo uploadFenceCreateInfo = vkinit::fenceCreateInfo();

	VK_CHECK(vkCreateFence(m_device, &uploadFenceCreateInfo, nullptr, &m_uploadContext.uploadFence));

	m_mainDeletionQueue.push_function([=, this]() {
		for (const auto& m_frame : m_frames)
		{
			vkDestroyFence(m_device, m_frame.renderFence, nullptr);

			vkDestroySemaphore(m_device, m_frame.presentSemaphore, nullptr);
			vkDestroySemaphore(m_device, m_frame.renderSemaphore, nullptr);
		}
		vkDestroyFence(m_device, m_uploadContext.uploadFence, nullptr);
		});
}

bool VulkanEngine::loadShaderModule(const char* filePath, VkShaderModule* outShaderModule) const
{
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
		return false;

	//find what the size of the file is by looking up the location of the cursor
	//because the cursor is at the end, it gives the size directly in bytes
	const std::streamsize fileSize = file.tellg();

	//spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	//put file cursor at beginning
	file.seekg(0);

	//load the entire file into the buffer
	file.read(reinterpret_cast<char*>(buffer.data()), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();

	//create a new shader module, using the buffer we loaded
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	//codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	//check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		std::cout << "Error when building " << filePath << " fragment shader module" << std::endl;
	}
	*outShaderModule = shaderModule;
	std::cout << filePath << " successfully loaded" << std::endl;

	return true;
}

void VulkanEngine::initPipelines()
{
	//compile mesh vertex shader
	VkShaderModule meshVertShader, meshFragShader, texturedMeshShader;
	loadShaderModule("../CompiledShaders/tri_mesh.vert.spv", &meshVertShader);
	loadShaderModule("../CompiledShaders/tri_mesh.frag.spv", &meshFragShader);
	loadShaderModule("../CompiledShaders/textured_lit.frag.spv", &texturedMeshShader);

	VkPipelineLayoutCreateInfo meshPipelineLayoutInfo = vkinit::pipelineLayoutCreateInfo();

	//setup push constants
	VkPushConstantRange push_constant;
	push_constant.offset = 0;
	push_constant.size = sizeof(MeshPushConstants);
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	meshPipelineLayoutInfo.pPushConstantRanges = &push_constant;
	meshPipelineLayoutInfo.pushConstantRangeCount = 1;

	VkDescriptorSetLayout setLayouts[] = { m_globalSetLayout, m_objectSetLayout };

	meshPipelineLayoutInfo.setLayoutCount = 2;
	meshPipelineLayoutInfo.pSetLayouts = setLayouts;

	VkPipelineLayout meshPipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(m_device, &meshPipelineLayoutInfo, nullptr, &meshPipelineLayout));


	//we start from  the normal mesh layout
	VkPipelineLayoutCreateInfo texturedPipelineLayoutInfo = meshPipelineLayoutInfo;

	VkDescriptorSetLayout texturedSetLayouts[] = { m_globalSetLayout, m_objectSetLayout, m_singleTextureSetLayout };

	texturedPipelineLayoutInfo.setLayoutCount = 3;
	texturedPipelineLayoutInfo.pSetLayouts = texturedSetLayouts;

	VkPipelineLayout texturedPipelineLayout;
	VK_CHECK(vkCreatePipelineLayout(m_device, &texturedPipelineLayoutInfo, nullptr, &texturedPipelineLayout));

	PipelineBuilder pipelineBuilder;

	pipelineBuilder.m_vertexInputInfo = vkinit::vertexInputStateCreateInfo();

	pipelineBuilder.m_inputAssembly = vkinit::inputAssemblyCreateInfo(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);

	pipelineBuilder.m_viewport.x = 0.0f;
	pipelineBuilder.m_viewport.y = 0.0f;
	pipelineBuilder.m_viewport.width = static_cast<float>(m_windowExtent.width);
	pipelineBuilder.m_viewport.height = static_cast<float>(m_windowExtent.height);
	pipelineBuilder.m_viewport.minDepth = 0.0f;
	pipelineBuilder.m_viewport.maxDepth = 1.0f;

	pipelineBuilder.m_scissor.offset = { 0, 0 };
	pipelineBuilder.m_scissor.extent = m_windowExtent;

	pipelineBuilder.m_rasterizer = vkinit::rasterizationStateCreateInfo(VK_POLYGON_MODE_FILL);
	pipelineBuilder.m_multisampling = vkinit::multisamplingStateCreateInfo();
	pipelineBuilder.m_colorBlendAttachment = vkinit::colorBlendAttachementState();
	pipelineBuilder.m_depthStencil = vkinit::depthStencilCreateInfo(true, true, VK_COMPARE_OP_LESS_OR_EQUAL);

	pipelineBuilder.m_pipelineLayout = meshPipelineLayout;

	VertexInputDescription vertexDescription = Vertex::getVertexDescription();

	//connect the pipeline builder vertex input info to the one we get from Vertex
	pipelineBuilder.m_vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineBuilder.m_vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexDescription.attributes.size());

	pipelineBuilder.m_vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineBuilder.m_vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexDescription.bindings.size());

	//clear the shader stages for the builder
	pipelineBuilder.m_shaderStages.clear();

	pipelineBuilder.m_shaderStages.push_back(
		vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
	pipelineBuilder.m_shaderStages.push_back(
		vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, meshFragShader));

	VkPipeline m_meshPipeline = pipelineBuilder.buildPipeline(m_device, m_renderPass);
	createMaterial(m_meshPipeline, meshPipelineLayout, "default");

	pipelineBuilder.m_pipelineLayout = texturedPipelineLayout;
	pipelineBuilder.m_shaderStages.clear();

	pipelineBuilder.m_shaderStages.push_back(
		vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT, meshVertShader));
	pipelineBuilder.m_shaderStages.push_back(
		vkinit::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT, texturedMeshShader));

	VkPipeline texPipeline = pipelineBuilder.buildPipeline(m_device, m_renderPass);
	createMaterial(texPipeline, texturedPipelineLayout, "texturedmesh");

	//deleting all of the vulkan shaders
	vkDestroyShaderModule(m_device, meshVertShader, nullptr);
	vkDestroyShaderModule(m_device, meshFragShader, nullptr);
	vkDestroyShaderModule(m_device, texturedMeshShader, nullptr);

	//adding the pipelines to the deletion queue
	m_mainDeletionQueue.push_function([=, this]()
		{
			vkDestroyPipeline(m_device, m_meshPipeline, nullptr);
			vkDestroyPipelineLayout(m_device, meshPipelineLayout, nullptr);
			vkDestroyPipeline(m_device, texPipeline, nullptr);
			vkDestroyPipelineLayout(m_device, texturedPipelineLayout, nullptr);
		});
}

void VulkanEngine::loadMeshes()
{
	Mesh bigguy{};
	bigguy.loadFromObj("../assets/bigguy.obj");
	m_meshes["bigguy"] = bigguy;

	Mesh monkey{};
	monkey.loadFromObj("../assets/monkey_smooth.obj");
	m_meshes["monkey"] = monkey;

	Mesh lostEmpire{};
	lostEmpire.loadFromObj("../assets/lost_empire.obj");
	m_meshes["lostEmpire"] = lostEmpire;

	/*Mesh bistro{};
	bistro.loadFromGltf("../assets/bistro/exterior.glb");*/

	uploadMesh(m_meshes["bigguy"]);
	uploadMesh(m_meshes["monkey"]);
	uploadMesh(m_meshes["lostEmpire"]);
}

void VulkanEngine::uploadMesh(Mesh& mesh)
{
	const size_t bufferSize = mesh.m_vertices.size() * sizeof(Vertex);

	//allocate vertex buffer
	VkBufferCreateInfo stagingBufferInfo = {};
	stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferInfo.pNext = nullptr;
	stagingBufferInfo.size = bufferSize;
	stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;


	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

	AllocatedBuffer stagingBuffer = {};

	VK_CHECK(vmaCreateBuffer(m_allocator, &stagingBufferInfo, &vmaallocInfo,
		&stagingBuffer.buffer,
		&stagingBuffer.allocation,
		nullptr));

	void* data;
	vmaMapMemory(m_allocator, stagingBuffer.allocation, &data);
	memcpy(data, mesh.m_vertices.data(), mesh.m_vertices.size() * sizeof(Vertex));
	vmaUnmapMemory(m_allocator, stagingBuffer.allocation);

	//allocate vertex buffer
	VkBufferCreateInfo vertexBufferInfo = {};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.pNext = nullptr;
	//this is the total size, in bytes, of the buffer we are allocating
	vertexBufferInfo.size = bufferSize;
	//this buffer is going to be used as a Vertex Buffer
	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	//let the VMA library know that this data should be GPU native
	vmaallocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	//allocate the buffer
	VK_CHECK(vmaCreateBuffer(m_allocator, &vertexBufferInfo, &vmaallocInfo,
		&mesh.m_vertexBuffer.buffer,
		&mesh.m_vertexBuffer.allocation,
		nullptr));

	immediateSubmit([=, this](const VkCommandBuffer &cmd) {
		VkBufferCopy copy;
		copy.dstOffset = 0;
		copy.srcOffset = 0;
		copy.size = bufferSize;
		vkCmdCopyBuffer(cmd, stagingBuffer.buffer, mesh.m_vertexBuffer.buffer, 1, &copy);
		});

	m_mainDeletionQueue.push_function([=, this]()
		{
			vmaDestroyBuffer(m_allocator, mesh.m_vertexBuffer.buffer, mesh.m_vertexBuffer.allocation);
		});
	vmaDestroyBuffer(m_allocator, stagingBuffer.buffer, stagingBuffer.allocation);
}

Material* VulkanEngine::createMaterial(VkPipeline pipeline, VkPipelineLayout layout, const std::string& name)
{
	Material mat;
	mat.pipeline = pipeline;
	mat.pipelineLayout = layout;
	m_materials[name] = mat;
	return &m_materials[name];
}

Material* VulkanEngine::getMaterial(const std::string& name)
{
	//search for the object, and return nullptr if not found
	const auto it = m_materials.find(name);
	return it == m_materials.end() ? nullptr : &(*it).second;	
}

Mesh* VulkanEngine::getMesh(const std::string& name)
{
	const auto it = m_meshes.find(name);
	return it == m_meshes.end() ? nullptr : &(*it).second;
	
}

void VulkanEngine::initScene()
{
	RenderObject bigguy{};
	bigguy.mesh = getMesh("bigguy");
	bigguy.material = getMaterial("default");
	glm::mat4 translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(0, -5.f, 0));
	glm::mat4 scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.5, 0.5, 0.5));
	bigguy.transformMatrix = translation * scale;

	m_renderables.push_back(bigguy);

	for (int x = -50; x <= 50; x++) {
		for (int y = -50; y <= 50; y++) {
			if (x * x + y * y > 20)
			{
				RenderObject monkey;
				monkey.mesh = getMesh("monkey");
				monkey.material = getMaterial("default");
				translation = glm::translate(glm::mat4{ 1.0 }, glm::vec3(x, -9.f, y));
				scale = glm::scale(glm::mat4{ 1.0 }, glm::vec3(0.2, 0.2, 0.2));
				monkey.transformMatrix = translation * scale;

				m_renderables.push_back(monkey);
			}
		}
	}

	RenderObject map;
	map.mesh = getMesh("lostEmpire");
	map.material = getMaterial("texturedmesh");
	map.transformMatrix = glm::translate(glm::vec3{ 5,-10,0 });

	m_renderables.push_back(map);

	//create a sampler for the texture
	const VkSamplerCreateInfo samplerInfo = vkinit::samplerCreateInfo(VK_FILTER_NEAREST);

	VkSampler blockySampler;
	vkCreateSampler(m_device, &samplerInfo, nullptr, &blockySampler);


	Material* texturedMat = getMaterial("texturedmesh");

	//allocate the descriptor set for single-texture to use on the material
	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.pNext = nullptr;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = m_descriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_singleTextureSetLayout;

	vkAllocateDescriptorSets(m_device, &allocInfo, &texturedMat->textureSet);

	//write to the descriptor set so that it points to our empire_diffuse texture
	VkDescriptorImageInfo imageBufferInfo;
	imageBufferInfo.sampler = blockySampler;
	imageBufferInfo.imageView = m_loadedTextures["empire_diffuse"].imageView;
	imageBufferInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	const VkWriteDescriptorSet texture1 = vkinit::writeDescriptorImage(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, texturedMat->textureSet, &imageBufferInfo, 0);

	vkUpdateDescriptorSets(m_device, 1, &texture1, 0, nullptr);

	m_camera.position = { -10.f, -11.f, 10.f };
	m_mainDeletionQueue.push_function([=, this]{
		vkDestroySampler(m_device, blockySampler, nullptr);
		});
}

void VulkanEngine::drawObjects(VkCommandBuffer cmd, RenderObject* first, const size_t count)
{
	GPUCameraData camData{};
	camData.proj = m_camera.getProjectionMatrix(false);
	camData.view = m_camera.getViewMatrix();
	camData.viewproj = camData.proj * camData.view;

	void* data;
	vmaMapMemory(m_allocator, getCurrentFrame().cameraBuffer.allocation, &data);
	memcpy(data, &camData, sizeof(GPUCameraData));
	vmaUnmapMemory(m_allocator, getCurrentFrame().cameraBuffer.allocation);

	const float framed = (static_cast<float>(m_frameNumber) / 120.f);
	m_sceneParameters.ambientColor = { sin(framed),0,cos(framed),1 };

	char* scene_data;
	vmaMapMemory(m_allocator, m_sceneParameterBuffer.allocation, reinterpret_cast<void**>(&scene_data));

	const int frame_index = static_cast<int>(static_cast<uint64_t>(m_frameNumber) % FRAME_OVERLAP);
	scene_data += padUniformBufferSize(sizeof(GPUSceneData)) * frame_index;
	memcpy(scene_data, &m_sceneParameters, sizeof(GPUSceneData));
	vmaUnmapMemory(m_allocator, m_sceneParameterBuffer.allocation);

	void* object_data;
	vmaMapMemory(m_allocator, getCurrentFrame().objectBuffer.allocation, &object_data);

	const auto objectSSBO = static_cast<GPUObjectData*>(object_data);

	for (unsigned i = 0; i < count; i++)
	{
		objectSSBO[i].modelMatrix = first[i].transformMatrix;
	}

	vmaUnmapMemory(m_allocator, getCurrentFrame().objectBuffer.allocation);

	const Mesh* lastMesh = nullptr;
	const Material* lastMaterial = nullptr;
	for (unsigned i = 0; i < count; i++)
	{
		const RenderObject& object = first[i];

		if (!object.material || !object.mesh)
			break;

		//only bind the pipeline if it doesn't match with the already bound one
		if (object.material != lastMaterial)
		{
			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipeline);
			lastMaterial = object.material;

			auto uniformOffset = static_cast<uint32_t>(padUniformBufferSize(sizeof(GPUSceneData)) * frame_index);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 0, 1, &getCurrentFrame().globalDescriptor, 1, &uniformOffset);

			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 1, 1, &getCurrentFrame().objectDescriptor, 0, nullptr);

			if (object.material->textureSet != VK_NULL_HANDLE)
				vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, object.material->pipelineLayout, 2, 1, &object.material->textureSet, 0, nullptr);
		}


		MeshPushConstants constants = { object.transformMatrix };

		//upload the mesh to the GPU via push constants
		vkCmdPushConstants(cmd, object.material->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &constants);

		//only bind the mesh if it's a different one from last bind
		if (object.mesh != lastMesh) {
			//bind the mesh vertex buffer with offset 0
			VkDeviceSize offset = 0;
			vkCmdBindVertexBuffers(cmd, 0, 1, &object.mesh->m_vertexBuffer.buffer, &offset);
			lastMesh = object.mesh;
		}
		//we can now draw
		vkCmdDraw(cmd, static_cast<uint32_t>(static_cast<uint32_t>(object.mesh->m_vertices.size())), 1, 0, i);
	}
}

FrameData& VulkanEngine::getCurrentFrame()
{
	return m_frames[m_frameNumber % FRAME_OVERLAP];
}

AllocatedBuffer VulkanEngine::createBuffer(const size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) const
{
	//allocate vertex buffer
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;

	bufferInfo.size = allocSize;
	bufferInfo.usage = usage;


	VmaAllocationCreateInfo vmaallocInfo = {};
	vmaallocInfo.usage = memoryUsage;

	AllocatedBuffer newBuffer{};

	//allocate the buffer
	VK_CHECK(vmaCreateBuffer(m_allocator, &bufferInfo, &vmaallocInfo,
		&newBuffer.buffer,
		&newBuffer.allocation,
		nullptr));
	return newBuffer;
}

void VulkanEngine::initDescriptors()
{
	//create a descriptor pool that will hold 10 uniform buffers
	std::vector<VkDescriptorPoolSize> sizes =
	{
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10 }
	};

	VkDeviceSize sceneParamBufferSize = FRAME_OVERLAP * padUniformBufferSize(sizeof(GPUSceneData));

	m_sceneParameterBuffer = createBuffer(sceneParamBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	VkDescriptorPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	pool_info.flags = 0;
	pool_info.maxSets = 10;
	pool_info.poolSizeCount = static_cast<uint32_t>(sizes.size());
	pool_info.pPoolSizes = sizes.data();

	vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptorPool);

	VkDescriptorSetLayoutBinding cameraBind = vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);
	VkDescriptorSetLayoutBinding sceneBind = vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 1);
	VkDescriptorSetLayoutBinding bindings[] = { cameraBind,sceneBind };

	VkDescriptorSetLayoutCreateInfo setinfo = {};
	setinfo.bindingCount = 2;
	setinfo.flags = 0;
	setinfo.pNext = nullptr;
	setinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setinfo.pBindings = bindings;

	vkCreateDescriptorSetLayout(m_device, &setinfo, nullptr, &m_globalSetLayout);

	VkDescriptorSetLayoutBinding objectBind = vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT, 0);

	VkDescriptorSetLayoutCreateInfo set2info = {};
	set2info.bindingCount = 1;
	set2info.flags = 0;
	set2info.pNext = nullptr;
	set2info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set2info.pBindings = &objectBind;

	vkCreateDescriptorSetLayout(m_device, &set2info, nullptr, &m_objectSetLayout);

	VkDescriptorSetLayoutBinding textureBind = vkinit::descriptorsetLayoutBinding(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 0);

	VkDescriptorSetLayoutCreateInfo set3info = {};
	set3info.bindingCount = 1;
	set3info.flags = 0;
	set3info.pNext = nullptr;
	set3info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	set3info.pBindings = &textureBind;

	vkCreateDescriptorSetLayout(m_device, &set3info, nullptr, &m_singleTextureSetLayout);

	for (auto& m_frame : m_frames)
	{
		m_frame.cameraBuffer = createBuffer(sizeof(GPUCameraData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
		m_frame.objectBuffer = createBuffer(sizeof(GPUObjectData) * MAX_OBJECTS, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

		//allocate one descriptor set for each frame
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.pNext = nullptr;
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = m_descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = &m_globalSetLayout;

		vkAllocateDescriptorSets(m_device, &allocInfo, &m_frame.globalDescriptor);

		VkDescriptorSetAllocateInfo objectSetAlloc = {};
		objectSetAlloc.pNext = nullptr;
		objectSetAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		objectSetAlloc.descriptorPool = m_descriptorPool;
		objectSetAlloc.descriptorSetCount = 1;
		objectSetAlloc.pSetLayouts = &m_objectSetLayout;

		vkAllocateDescriptorSets(m_device, &objectSetAlloc, &m_frame.objectDescriptor);


		VkDescriptorBufferInfo cameraInfo;
		cameraInfo.buffer = m_frame.cameraBuffer.buffer;
		cameraInfo.offset = 0;
		cameraInfo.range = sizeof(GPUCameraData);

		VkDescriptorBufferInfo sceneInfo;
		sceneInfo.buffer = m_sceneParameterBuffer.buffer;
		sceneInfo.offset = 0;
		sceneInfo.range = sizeof(GPUSceneData);

		VkDescriptorBufferInfo objectBufferInfo;
		objectBufferInfo.buffer = m_frame.objectBuffer.buffer;
		objectBufferInfo.offset = 0;
		objectBufferInfo.range = sizeof(GPUObjectData) * MAX_OBJECTS;

		VkWriteDescriptorSet cameraWrite = vkinit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, m_frame.globalDescriptor, &cameraInfo, 0);
		VkWriteDescriptorSet sceneWrite = vkinit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
		                                                                m_frame.globalDescriptor, &sceneInfo, 1);
		VkWriteDescriptorSet objectWrite = vkinit::writeDescriptorBuffer(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, m_frame.objectDescriptor, &objectBufferInfo, 0);

		VkWriteDescriptorSet setWrites[] = { cameraWrite,sceneWrite,objectWrite };

		vkUpdateDescriptorSets(m_device, 3, setWrites, 0, nullptr);
	}
	// add buffers to deletion queues
	m_mainDeletionQueue.push_function([=, this]()
		{
			for (auto& m_frame : m_frames)
			{
				vmaDestroyBuffer(m_allocator, m_frame.cameraBuffer.buffer, m_frame.cameraBuffer.allocation);
				vmaDestroyBuffer(m_allocator, m_frame.objectBuffer.buffer, m_frame.objectBuffer.allocation);
			}

			vkDestroyDescriptorSetLayout(m_device, m_globalSetLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_device, m_objectSetLayout, nullptr);
			vkDestroyDescriptorSetLayout(m_device, m_singleTextureSetLayout, nullptr);

			vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
			vmaDestroyBuffer(m_allocator, m_sceneParameterBuffer.buffer, m_sceneParameterBuffer.allocation);

		});


}

VkDeviceSize VulkanEngine::padUniformBufferSize(size_t originalSize) const
{
	// Calculate required alignment based on minimum device offset alignment
	const VkDeviceSize minUboAlignment = m_gpuProperties.limits.minUniformBufferOffsetAlignment;
	VkDeviceSize alignedSize = originalSize;
	if (minUboAlignment > 0) {
		alignedSize = (alignedSize + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}
	return alignedSize;
}

void VulkanEngine::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) const
{
	VkCommandBuffer cmd = m_uploadContext.commandBuffer;

	//begin the command buffer recording. We will use this command buffer exactly once before resetting, so we tell vulkan that
	const VkCommandBufferBeginInfo cmdBeginInfo = vkinit::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	//execute the function
	function(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	const VkSubmitInfo submit = vkinit::submitInfo(&cmd);

	//submit command buffer to the queue and execute it.
	// _uploadFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submit, m_uploadContext.uploadFence));

	vkWaitForFences(m_device, 1, &m_uploadContext.uploadFence, true, INT_MAX);
	vkResetFences(m_device, 1, &m_uploadContext.uploadFence);

	// reset the command buffers inside the command pool
	vkResetCommandPool(m_device, m_uploadContext.commandPool, 0);
}

void VulkanEngine::loadImages()
{
	Texture lostEmpire{};

	vkutil::loadImageFromFile(*this, "../assets/lost_empire-RGBA.png", lostEmpire.image);

	const VkImageViewCreateInfo imageinfo = vkinit::imageviewCreateInfo(VK_FORMAT_R8G8B8A8_SRGB, lostEmpire.image.image, VK_IMAGE_ASPECT_COLOR_BIT);
	vkCreateImageView(m_device, &imageinfo, nullptr, &lostEmpire.imageView);
	m_loadedTextures["empire_diffuse"] = lostEmpire;

	m_mainDeletionQueue.push_function([=, this]
		{
			vkDestroyImageView(m_device, lostEmpire.imageView, nullptr);
		});
}

