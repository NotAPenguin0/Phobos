#ifndef PHOBOS_FIXED_GENERIC_RENDERER_HPP_
#define PHOBOS_FIXED_GENERIC_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/renderer/mesh.hpp>
#include <phobos/renderer/command_buffer.hpp>
#include <phobos/renderer/renderer.hpp>

#include <vector>

#include <glm/mat4x4.hpp>

namespace ph::fixed {

class GenericRenderer {
public:
	GenericRenderer(VulkanContext* ctx, Renderer* renderer) : ctx(ctx), renderer(renderer) {}

	uint32_t add_material(Material const& mat);
	void add_draw(Mesh* mesh, uint32_t material_index, glm::mat4 const& transform);
	void execute(FrameInfo& frame, CommandBuffer& cmd_buf);

private:
	struct Draw {
		Mesh* mesh = nullptr;
		uint32_t material_index = 0;
		uint32_t transform_index = 0;
	};

	VulkanContext* ctx = nullptr;
	Renderer* renderer = nullptr;

	std::vector<Draw> draws;
	std::vector<glm::mat4> transforms;
	std::vector<Material> materials;

	BufferSlice transform_ssbo;

	void update_transforms(CommandBuffer& cmd_buf);
	vk::DescriptorSet get_descriptor_set(FrameInfo& frame, ph::RenderPass& pass);
};

}

#endif