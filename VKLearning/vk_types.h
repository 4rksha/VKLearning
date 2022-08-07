// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once
#include <vulkan/vulkan.h>
#if _DEBUG
#define VMA_RECORDING_ENABLED 1
#endif
#include <vk_mem_alloc.h>

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
