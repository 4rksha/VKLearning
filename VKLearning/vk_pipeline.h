#pragma once

#include "vk_types.h"
#include <vector>

class PipelineBuilder
{

public:
	VkPipeline buildPipeline(VkDevice device, VkRenderPass pass);

public:

	std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;
	VkPipelineVertexInputStateCreateInfo		 m_vertexInputInfo;
	VkPipelineInputAssemblyStateCreateInfo		 m_inputAssembly;
	VkViewport									 m_viewport;
	VkRect2D									 m_scissor;
	VkPipelineRasterizationStateCreateInfo		 m_rasterizer;
	VkPipelineColorBlendAttachmentState			 m_colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo		 m_multisampling;
	VkPipelineLayout							 m_pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo		 m_depthStencil;

};
