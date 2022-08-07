#pragma once

#include "vk_types.h"
#include "vk_engine.h"


#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl.h>

class VulkanUI
{
public:
	VulkanUI() = delete;

	static void init(VulkanEngine *engine);
	static void updateImGui(VulkanEngine* engine);

	static void appMainMenuBar();
	static void menuFile();
	static void overlay(const double fps);
private:
	
};