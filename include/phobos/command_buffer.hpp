#pragma once

#include <vulkan/vulkan.h>
#include <string_view>

#include <plib/bit_flag.hpp>

#include <phobos/pass.hpp>
#include <phobos/pipeline.hpp>
#include <phobos/buffer.hpp>

namespace ph {

class Context;
class Queue;

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
	CommandBuffer& bind_compute_pipeline(std::string_view name);
	CommandBuffer& bind_descriptor_set(VkDescriptorSet set);

	CommandBuffer& bind_vertex_buffer(uint32_t first_binding, VkBuffer buffer, VkDeviceSize offset);
	CommandBuffer& bind_vertex_buffer(uint32_t first_binding, BufferSlice slice);

	// Sets viewport and scissor regions to the current framebuffer size.
	// Only valid if viewport and scissor are dynamic states of the current pipeline
	CommandBuffer& auto_viewport_scissor();

	CommandBuffer& draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);

	CommandBuffer& barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkBufferMemoryBarrier const& barrier, VkDependencyFlags dependency = VK_DEPENDENCY_BY_REGION_BIT);
	CommandBuffer& barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkImageMemoryBarrier const& barrier, VkDependencyFlags dependency = VK_DEPENDENCY_BY_REGION_BIT);
	CommandBuffer& barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkMemoryBarrier const& barrier, VkDependencyFlags dependency = VK_DEPENDENCY_BY_REGION_BIT);

	CommandBuffer& transition_layout(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::ResourceAccess> src_access, plib::bit_flag<ph::PipelineStage> dst_stage, plib::bit_flag<ph::ResourceAccess> dst_access, 
		ph::ImageView const& view, VkImageLayout old_layout, VkImageLayout new_layout);

	CommandBuffer& transition_layout(plib::bit_flag<ph::PipelineStage> src_stage, VkAccessFlags src_access, plib::bit_flag<ph::PipelineStage> dst_stage, VkAccessFlags dst_access,
		ph::ImageView const& view, VkImageLayout old_layout, VkImageLayout new_layout);

	CommandBuffer& dispatch(uint32_t x, uint32_t y, uint32_t z);

	CommandBuffer& release_ownership(Queue const& src, Queue const& dst, ph::ImageView const& image, VkImageLayout final_layout);
	CommandBuffer& acquire_ownership(Queue const& src, Queue const& dst, ph::ImageView const& image, VkImageLayout final_layout);

	VkCommandBuffer handle() const;

private:
	Context* ctx = nullptr;
	VkCommandBuffer cmd_buf = nullptr;
	VkRenderPass cur_renderpass = nullptr;
	VkRect2D cur_render_area = {};
	Pipeline cur_pipeline{};
};

}