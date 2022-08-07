#pragma once

#include "vk_types.h"
#include <vector>
#include <glm/glm.hpp>

using Color = glm::vec4;

struct VertexInputDescription 
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;

    VkPipelineVertexInputStateCreateFlags flags = 0;
};

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    Color color;
    glm::vec2 uv;

    static VertexInputDescription getVertexDescription();
};

class Mesh
{
public:
    bool loadFromObj(const char* filename);
    bool loadFromGltf(const char* filename);
public:
    std::vector<Vertex> m_vertices;
    AllocatedBuffer m_vertexBuffer;
};

