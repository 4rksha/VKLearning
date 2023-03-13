#include "vk_mesh.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>
#include <iostream>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#include "tiny_gltf.h"

VertexInputDescription Vertex::getVertexDescription()
{
	VertexInputDescription description;

	VkVertexInputBindingDescription mainBinding = {};
	mainBinding.binding = 0;
	mainBinding.stride = sizeof(Vertex);
	mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	description.bindings.push_back(mainBinding);

	VkVertexInputAttributeDescription positionAttribute = {};
	positionAttribute.binding = 0;
	positionAttribute.location = 0;
	positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	positionAttribute.offset = offsetof(Vertex, position);

	VkVertexInputAttributeDescription normalAttribute = {};
	normalAttribute.binding = 0;
	normalAttribute.location = 1;
	normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
	normalAttribute.offset = offsetof(Vertex, normal);

	VkVertexInputAttributeDescription colorAttribute = {};
	colorAttribute.binding = 0;
	colorAttribute.location = 2;
	colorAttribute.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	colorAttribute.offset = offsetof(Vertex, color);

	VkVertexInputAttributeDescription uvAttribute = {};
	uvAttribute.binding = 0;
	uvAttribute.location = 3;
	uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
	uvAttribute.offset = offsetof(Vertex, uv);


	description.attributes.push_back(positionAttribute);
	description.attributes.push_back(normalAttribute);
	description.attributes.push_back(colorAttribute);
	description.attributes.push_back(uvAttribute);
	return description;
}

bool Mesh::loadFromObj(const char* filename)
{
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;

	tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);

	if (!warn.empty()) {
		std::cout << "WARN: " << warn << std::endl;
	}
	if (!err.empty()) {
		std::cerr << err << std::endl;
		return false;
	}

	// Loop over shapes
	for (auto &shape : shapes) {
		// Loop over faces(polygon)
		size_t index_offset = 0;
		for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {

			//hardcode loading to triangles
			int fv = 3;
			int mat_index = shape.mesh.material_ids[f];
			// Loop over vertices in the face.
			for (size_t v = 0; v < fv; v++) {

				// access to vertex
				tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

				//vertex position
				int index = 3 * idx.vertex_index;
				tinyobj::real_t vx = attrib.vertices[index];
				tinyobj::real_t vy = attrib.vertices[1 + index];
				tinyobj::real_t vz = attrib.vertices[2 + index];

				//vertex normal
				index = 3 * idx.normal_index;
				tinyobj::real_t nx = attrib.normals[index];
				tinyobj::real_t ny = attrib.normals[1 + index];
				tinyobj::real_t nz = attrib.normals[2 + index];

				//vertex uv
				index = 2 * idx.texcoord_index;
				tinyobj::real_t ux = attrib.texcoords[index];
				tinyobj::real_t uy = attrib.texcoords[1 + index];


				//copy it into our vertex
				Vertex new_vert{};
				new_vert.position.x = vx;
				new_vert.position.y = vy;
				new_vert.position.z = vz;

				new_vert.normal.x = nx;
				new_vert.normal.y = ny;
				new_vert.normal.z = nz;

				new_vert.uv.x = ux;
				new_vert.uv.y = 1 - uy;

				//we are setting the vertex color as the vertex normal. This is just for display purposes
				new_vert.color = glm::vec4(1.f);
				m_vertices.push_back(new_vert);
			}
			index_offset += fv;
		}
	}
	return true;
}

bool Mesh::loadFromGltf(const char* filename)
{
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;
	tinygltf::Model model;

	const bool res = loader.LoadBinaryFromFile(&model, &err, &warn, filename);
	if (!warn.empty()) {
		std::cout << "WARN: " << warn << std::endl;
	}

	if (!err.empty()) {
		std::cout << "ERR: " << err << std::endl;
	}

	if (!res)
		std::cout << "Failed to load glTF: " << filename << std::endl;
	else
		std::cout << "Loaded glTF: " << filename << std::endl;

	return res;
}
