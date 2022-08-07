#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

class DescriptorAllocator
{
public:
	struct PoolSizes {
		std::vector<std::pair<VkDescriptorType, uint32_t>> sizes =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 8 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 2 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 2 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 2 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 }
		};
	};

	void init(VkDevice device);
	void cleanup();
	VkDescriptorPool createPool(VkDevice device, const PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags);
	void resetPools();
	bool allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout);

	VkDevice m_device = VK_NULL_HANDLE;
private:
	VkDescriptorPool grabPool();

	VkDescriptorPool m_currentPool{ VK_NULL_HANDLE };
	PoolSizes m_descriptorSizes;
	std::vector<VkDescriptorPool> m_activePools;
	std::vector<VkDescriptorPool> m_freePools;

};

class DescriptorLayoutCache {
public:
	void init(VkDevice device);
	void cleanup();

	VkDescriptorSetLayout create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info);

	struct DescriptorLayoutInfo {
		//good idea to turn this into a inlined array
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		bool operator==(const DescriptorLayoutInfo& other) const;

		size_t hash() const;
	};


private:

	struct DescriptorLayoutHash {

		std::size_t operator()(const DescriptorLayoutInfo& k) const {
			return k.hash();
		}
	};

	std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHash> m_layoutCache;
	VkDevice m_device = VK_NULL_HANDLE;
};

class DescriptorBuilder {
public:
	static DescriptorBuilder begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator );

	DescriptorBuilder& bindBuffer(uint32_t binding, VkDescriptorBufferInfo* bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);
	DescriptorBuilder& bindImage(uint32_t binding, VkDescriptorImageInfo* imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags);

	bool build(VkDescriptorSet& set, VkDescriptorSetLayout& layout);
	bool build(VkDescriptorSet& set);
private:

	std::vector<VkWriteDescriptorSet> m_writes;
	std::vector<VkDescriptorSetLayoutBinding> m_bindings;

	DescriptorLayoutCache* m_pCache {nullptr};
	DescriptorAllocator* m_pAlloc {nullptr};
	};