#include "vk_descriptors.h"
#include <algorithm>


void DescriptorAllocator::init(VkDevice device)
{
	m_device = device;
}

void DescriptorAllocator::cleanup()
{
	for (auto p : m_activePools)
	{
		vkDestroyDescriptorPool(m_device, p, nullptr);
	}
	for (auto p : m_freePools)
	{
		vkDestroyDescriptorPool(m_device, p, nullptr);
	}
}

VkDescriptorPool DescriptorAllocator::createPool(VkDevice device, const PoolSizes& poolSizes, const int count, const VkDescriptorPoolCreateFlags flags)
{
	std::vector<VkDescriptorPoolSize> sizes;
	sizes.reserve(poolSizes.sizes.size());
	for (const auto size : poolSizes.sizes)
	{
		sizes.push_back({ size.first, static_cast<uint32_t>(size.second * count) });
	}
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = flags;
	poolInfo.maxSets = count;
	poolInfo.poolSizeCount = (uint32_t)sizes.size();
	poolInfo.pPoolSizes = sizes.data();

	VkDescriptorPool descriptorPool;
	vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &descriptorPool);

	return descriptorPool;
}

VkDescriptorPool DescriptorAllocator::grabPool()
{
	if (!m_freePools.empty())
	{
		VkDescriptorPool pool = m_freePools.back();
		m_freePools.pop_back();
		return pool;
	}
	return createPool(m_device, m_descriptorSizes, 500, 0);
}

void DescriptorLayoutCache::init(VkDevice device)
{
	m_device = device;
}

void DescriptorLayoutCache::cleanup()
{
	for (auto pair : m_layoutCache)
	{
		vkDestroyDescriptorSetLayout(m_device, pair.second, nullptr);
	}
}

VkDescriptorSetLayout DescriptorLayoutCache::create_descriptor_layout(VkDescriptorSetLayoutCreateInfo* info)
{
	DescriptorLayoutInfo layoutInfo;
	layoutInfo.bindings.reserve(info->bindingCount);
	bool isSorted = true;
	uint32_t lastBinding = -1;

	for (uint32_t i = 0; i < info->bindingCount; ++i)
	{
		layoutInfo.bindings.push_back(info->pBindings[i]);
		if (info->pBindings[i].binding > lastBinding)
		{
			lastBinding = info->pBindings[i].binding;
		}
		else
		{
			isSorted = false;
		}
	}

	if (!isSorted)
	{
		std::sort(layoutInfo.bindings.begin(), layoutInfo.bindings.end(), [](VkDescriptorSetLayoutBinding& a, VkDescriptorSetLayoutBinding& b) {
			return a.binding < b.binding;
			});
	}

	auto it = m_layoutCache.find(layoutInfo);
	if (it != m_layoutCache.end())
		return (*it).second;

	VkDescriptorSetLayout layout;
	vkCreateDescriptorSetLayout(m_device, info, nullptr, &layout);
	m_layoutCache[layoutInfo] = layout;
	return layout;
}

bool DescriptorLayoutCache::DescriptorLayoutInfo::operator==(const DescriptorLayoutInfo& other) const
{
	if (other.bindings.size() != bindings.size())
		return false;

	for (std::size_t i = 0; i < bindings.size(); ++i)
	{
		if (other.bindings[i].binding != bindings[i].binding)
			return false;
		if (other.bindings[i].descriptorType != bindings[i].descriptorType)
			return false;
		if (other.bindings[i].descriptorCount != bindings[i].descriptorCount)
			return false;
		if (other.bindings[i].stageFlags != bindings[i].stageFlags)
			return false;
	}
	return true;
}

size_t DescriptorLayoutCache::DescriptorLayoutInfo::hash() const
{
	std::size_t result = std::hash<std::size_t>()(bindings.size());

	for (const VkDescriptorSetLayoutBinding& b : bindings)
	{
		std::size_t bindingHash = b.binding | b.descriptorType << 8 | b.descriptorCount << 16 | b.stageFlags << 24;
		result ^= std::hash<std::size_t>()(bindingHash);
	}
	return result;
}

DescriptorBuilder DescriptorBuilder::begin(DescriptorLayoutCache* layoutCache, DescriptorAllocator* allocator)
{
	DescriptorBuilder builder;
	builder.m_pCache = layoutCache;
	builder.m_pAlloc = allocator;
	return builder;
}

DescriptorBuilder& DescriptorBuilder::bindBuffer(const uint32_t binding, VkDescriptorBufferInfo* bufferInfo, const VkDescriptorType type, const VkShaderStageFlags stageFlags)
{
	//create the descriptor binding for the layout
	VkDescriptorSetLayoutBinding newBinding{};

	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	m_bindings.push_back(newBinding);

	//create the descriptor write
	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pBufferInfo = bufferInfo;
	newWrite.dstBinding = binding;

	m_writes.push_back(newWrite);
	return *this;
}

DescriptorBuilder& DescriptorBuilder::bindImage(const uint32_t binding, VkDescriptorImageInfo* imageInfo, const VkDescriptorType type, const VkShaderStageFlags stageFlags)
{
	VkDescriptorSetLayoutBinding newBinding{};

	newBinding.descriptorCount = 1;
	newBinding.descriptorType = type;
	newBinding.pImmutableSamplers = nullptr;
	newBinding.stageFlags = stageFlags;
	newBinding.binding = binding;

	m_bindings.push_back(newBinding);

	VkWriteDescriptorSet newWrite{};
	newWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	newWrite.pNext = nullptr;

	newWrite.descriptorCount = 1;
	newWrite.descriptorType = type;
	newWrite.pImageInfo = imageInfo;
	newWrite.dstBinding = binding;

	m_writes.push_back(newWrite);
	return *this;
}

bool DescriptorBuilder::build(VkDescriptorSet& set, VkDescriptorSetLayout& layout)
{
	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.pBindings = m_bindings.data();
	layoutInfo.bindingCount = static_cast<uint32_t>(m_bindings.size());

	layout = m_pCache->create_descriptor_layout(&layoutInfo);

	if (!m_pAlloc->allocate(&set, layout))
		return false;

	for (VkWriteDescriptorSet& w : m_writes)
	{
		w.dstSet = set;
	}

	vkUpdateDescriptorSets(m_pAlloc->m_device, static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);

	return true;
}

bool DescriptorBuilder::build(VkDescriptorSet& set)
{
	VkDescriptorSetLayout layout;
	return build(set, layout);
}

bool DescriptorAllocator::allocate(VkDescriptorSet* set, VkDescriptorSetLayout layout)
{
	if (m_currentPool == VK_NULL_HANDLE)
	{
		m_currentPool = grabPool();
		m_activePools.push_back(m_currentPool);
	}

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.pSetLayouts = &layout;
	allocInfo.descriptorPool = m_currentPool;
	allocInfo.descriptorSetCount = 1;

	VkResult allocResult = vkAllocateDescriptorSets(m_device, &allocInfo, set);
	bool needReallocate = false;

	switch (allocResult)
	{
	case VK_SUCCESS:
		return true;
	case VK_ERROR_FRAGMENTED_POOL:
	case VK_ERROR_OUT_OF_POOL_MEMORY:
		needReallocate = true;
		break;
	default:
		return false;
	}

	if (needReallocate)
	{
		m_currentPool = grabPool();
		m_activePools.push_back(m_currentPool);

		allocResult = vkAllocateDescriptorSets(m_device, &allocInfo, set);
	}
	return allocResult == VK_SUCCESS;
}


void DescriptorAllocator::resetPools()
{
	for (auto p : m_activePools)
	{
		vkResetDescriptorPool(m_device, p, 0);
		m_freePools.push_back(p);
	}

	m_activePools.clear();

	m_currentPool = VK_NULL_HANDLE;
}

