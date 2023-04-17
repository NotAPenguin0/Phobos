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
	CommandBuffer(CommandBuffer&& rhs) = default;
	CommandBuffer(CommandBuffer const& rhs) = delete;
	CommandBuffer(Context& context, VkCommandBuffer&& cmd_buf);

	CommandBuffer& operator=(CommandBuffer&& rhs) = default;

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

	CommandBuffer& bind_index_buffer(BufferSlice slice, VkIndexType type);

	CommandBuffer& push_constants(plib::bit_flag<ph::ShaderStage> stage, uint32_t offset, uint32_t size, void const* data);

	// Sets viewport and scissor regions to the current framebuffer size.
	// Only valid if viewport and scissor are dynamic states of the current pipeline
	CommandBuffer& auto_viewport_scissor();
	CommandBuffer& set_viewport(VkViewport vp);
	CommandBuffer& set_scissor(VkRect2D scissor);

	CommandBuffer& draw(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);
	CommandBuffer& draw_indexed(uint32_t index_count, uint32_t instance_count = 1, uint32_t first_index = 0, int32_t vertex_offset = 0, uint32_t first_instance = 0);

	CommandBuffer& barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkBufferMemoryBarrier const& barrier, VkDependencyFlags dependency = VK_DEPENDENCY_BY_REGION_BIT);
	CommandBuffer& barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkImageMemoryBarrier const& barrier, VkDependencyFlags dependency = VK_DEPENDENCY_BY_REGION_BIT);
	CommandBuffer& barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkMemoryBarrier const& barrier, VkDependencyFlags dependency = VK_DEPENDENCY_BY_REGION_BIT);

	CommandBuffer& transition_layout(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::ResourceAccess> src_access, plib::bit_flag<ph::PipelineStage> dst_stage, plib::bit_flag<ph::ResourceAccess> dst_access, 
		ph::ImageView const& view, VkImageLayout old_layout, VkImageLayout new_layout);

	CommandBuffer& transition_layout(plib::bit_flag<ph::PipelineStage> src_stage, VkAccessFlags src_access, plib::bit_flag<ph::PipelineStage> dst_stage, VkAccessFlags dst_access,
		ph::ImageView const& view, VkImageLayout old_layout, VkImageLayout new_layout);

	CommandBuffer& dispatch(uint32_t x, uint32_t y, uint32_t z);

	// Doesn't work. Don't use anyway since images have concurrent sharing mode
	CommandBuffer& release_ownership(Queue const& src, Queue const& dst, ph::ImageView const& image, VkImageLayout final_layout);
	CommandBuffer& acquire_ownership(Queue const& src, Queue const& dst, ph::ImageView const& image, VkImageLayout final_layout);
	
	// Doens't work. Also no need because concurrent sharing mode
	CommandBuffer& release_ownership(Queue const& src, Queue const& dst, ph::RawBuffer const& buffer);
	CommandBuffer& acquire_ownership(Queue const& src, Queue const& dst, ph::RawBuffer const& buffer);

	CommandBuffer& copy_buffer(BufferSlice src, BufferSlice dst);
	CommandBuffer& copy_buffer_to_image(BufferSlice src, ph::ImageView dst, VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CommandBuffer& copy_image_to_buffer(ph::ImageView src, BufferSlice dst, VkImageLayout layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    CommandBuffer& blit_image(ph::RawImage const& src, VkImageLayout src_layout, ph::RawImage const& dst, VkImageLayout dst_layout, VkImageBlit blit, VkFilter filter = VK_FILTER_LINEAR);

#if PHOBOS_ENABLE_RAY_TRACING
	CommandBuffer& bind_ray_tracing_pipeline(std::string_view name);

	CommandBuffer& build_acceleration_structure(VkAccelerationStructureBuildGeometryInfoKHR const& info, VkAccelerationStructureBuildRangeInfoKHR const* ranges);
	CommandBuffer& write_acceleration_structure_properties(VkAccelerationStructureKHR as, VkQueryType query_type, VkQueryPool query_pool, uint32_t index);
    CommandBuffer& write_acceleration_structure_properties(std::span<VkAccelerationStructureKHR> handles, VkQueryType query_type, VkQueryPool query_pool, uint32_t first);
	CommandBuffer& copy_acceleration_structure(VkAccelerationStructureKHR src, VkAccelerationStructureKHR dst, VkCopyAccelerationStructureModeKHR mode);
	CommandBuffer& compact_acceleration_structure(VkAccelerationStructureKHR src, VkAccelerationStructureKHR dst);

	CommandBuffer& trace_rays(ShaderBindingTable const& sbt, uint32_t x, uint32_t y, uint32_t z);
#endif

	CommandBuffer& reset_query_pool(VkQueryPool pool, uint32_t first = 0, uint32_t count = 1);

	VkCommandBuffer handle() const;

private:
	Context* ctx = nullptr;
	VkCommandBuffer cmd_buf = nullptr;
	VkRenderPass cur_renderpass = nullptr;
	VkRect2D cur_render_area = {};
	Pipeline cur_pipeline{};
};

}