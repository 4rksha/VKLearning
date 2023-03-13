#include <iostream>
#include "vk_ui.h"
#include "vk_utils.h"
#include "string"

void VulkanUI::init(VulkanEngine* engine)
{
	//1: create descriptor pool for IMGUI
	// the size of the pool is very oversize, but it's copied from imgui demo itself.
	constexpr VkDescriptorPoolSize pool_sizes[] =
	{
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
	};

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	poolInfo.maxSets = 1000;
	poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
	poolInfo.pPoolSizes = pool_sizes;

	VkDescriptorPool imguiPool;
	VK_CHECK(vkCreateDescriptorPool(engine->m_device, &poolInfo, nullptr, &imguiPool));


	// 2: initialize imgui library

	//this initializes the core structures of imgui
	ImGui::CreateContext();

	//this initializes imgui for SDL
	ImGui_ImplSDL2_InitForVulkan(engine->m_window);

	//this initializes imgui for Vulkan
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = engine->m_instance;
	initInfo.PhysicalDevice = engine->m_chosenGPU;
	initInfo.Device = engine->m_device;
	initInfo.Queue = engine->m_graphicsQueue;
	initInfo.DescriptorPool = imguiPool;
	initInfo.MinImageCount = 3;
	initInfo.ImageCount = 3;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

	ImGui_ImplVulkan_Init(&initInfo, engine->m_renderPass);
	//execute a gpu command to upload imgui font textures
	engine->immediateSubmit([&](const VkCommandBuffer& cmd) {
		ImGui_ImplVulkan_CreateFontsTexture(cmd);
		});

	//clear font textures from cpu data
	ImGui_ImplVulkan_DestroyFontUploadObjects();

	//add the destroy the imgui created structures
	engine->m_mainDeletionQueue.push_function([=]() {

		vkDestroyDescriptorPool(engine->m_device, imguiPool, nullptr);
		ImGui_ImplVulkan_Shutdown();
		});
}

void VulkanUI::updateImGui(VulkanEngine* engine)
{
	//imgui new frame
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL2_NewFrame(engine->m_window);

	ImGui::NewFrame();
	//ImGui::ShowDemoWindow();
	appMainMenuBar();
	leftPanel(engine);
}

void VulkanUI::appMainMenuBar()
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void VulkanUI::bottomInfo(VulkanEngine* engine)
{
	ImGui::Text("GPU + CPU : %3.1f ms, %3.1f fps", engine->getMeanDeltaTime(), 1000.f / engine->getMeanDeltaTime());
	ImGui::Separator();
}

void VulkanUI::leftPanel(VulkanEngine* engine)
{
	const ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse;

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
	const ImVec2 work_size = viewport->WorkSize;
	ImVec2 window_pos, window_size;
	window_size.x = work_size.x / 5.f;
	window_size.y = work_size.y;

	ImGui::SetNextWindowPos(work_pos);
	ImGui::SetNextWindowSize(window_size);
	ImGui::SetNextWindowBgAlpha(1.f); // Transparent background
	 
	if (ImGui::Begin("LeftPanel", nullptr, window_flags))
	{
		bottomInfo(engine);
		ImGui::Separator();
		ImGui::Text("GGX Params");
		GPUSceneData *params = &engine->m_sceneParameters;
		ImGui::DragFloat3("albedo", &(params->albedo[0]),0.01f, 0.0f, 1.0f, "%.3f");
		ImGui::DragFloat("metallic", &(params->metallic),0.01f, 0.0f, 1.0f, "%.3f");
		ImGui::DragFloat("roughness", &(params->roughness),0.01f, 0.0f, 1.0f, "%.3f");
		ImGui::DragInt("light number", &(params->lightNb),1, 1, 1000, "%.3f");
		for (int i = 0; i < params->lightNb; ++i)
		{
			ImGui::Separator();
			std::string label0 = "Light" + std::to_string(i);
			ImGui::Text(label0.c_str());

			std::string label1 = "position" + std::to_string(i);
			ImGui::DragFloat3(label1.c_str(), &(engine->m_lightData[i].position[0]));

			std::string label2 = "intensity" + std::to_string(i);
			ImGui::DragFloat(label2.c_str(), &(engine->m_lightData[i].intensity),0.01f, 0.f, 100.f, "%.2f");

			std::string label3 = "color" + std::to_string(i);
			ImGui::DragFloat3(label3.c_str(), &(engine->m_lightData[i].color[0]), 0.01f, 0.f, 1.f);
		}

	}
	ImGui::End();
}