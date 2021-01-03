#pragma once

#include <vulkan/vulkan.h>
#include <string_view>

#include <phobos/pipeline.hpp>

namespace ph {

class Context;

class CommandBuffer {
public:
	CommandBuffer() = default;
	CommandBuffer(Context& context, VkCommandBuffer&& cmd_buf);

	CommandBuffer& begin(VkCommandBufferUsageFlags flags = {});
	CommandBuffer& end();

	CommandBuffer& begin_renderpass(VkRenderPassBeginInfo const& info);
	CommandBuffer& end_renderpass();

	Pipeline const& get_bound_pipeline() const;
	CommandBuffer& bind_pipeline(std::string_view name);
	CommandBuffer& bind_descriptor_set(VkDescriptorSet set);

	// Sets viewport and scissor regions to the current framebuffer size.
	// Only valid if viewport and scissor are dynamic states of the current pipeline
	CommandBuffer& auto_viewport_scissor();

	CommandBuffer& draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);

	VkCommandBuffer handle();

private:
	Context* ctx = nullptr;
	VkCommandBuffer cmd_buf = nullptr;
	VkRenderPass cur_renderpass = nullptr;
	VkRect2D cur_render_area = {};
	Pipeline cur_pipeline{};
};

}