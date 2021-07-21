#include <phobos/command_buffer.hpp>
#include <phobos/context.hpp>
#include <phobos/queue.hpp>

#include <cassert>

namespace ph {

CommandBuffer::CommandBuffer(Context& context, VkCommandBuffer&& cmd_buf) : ctx(&context), cmd_buf(cmd_buf) {

}

CommandBuffer& CommandBuffer::begin(VkCommandBufferUsageFlags flags) {
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = flags;
	vkBeginCommandBuffer(cmd_buf, &begin_info);
	return *this;
}

CommandBuffer& CommandBuffer::end() {
	vkEndCommandBuffer(cmd_buf);
	return *this;
}

CommandBuffer& CommandBuffer::begin_renderpass(VkRenderPassBeginInfo const& info) {
	vkCmdBeginRenderPass(cmd_buf, &info, VK_SUBPASS_CONTENTS_INLINE);
	cur_renderpass = info.renderPass;
	cur_render_area = info.renderArea;
	return *this;
}

CommandBuffer& CommandBuffer::end_renderpass() {
	vkCmdEndRenderPass(cmd_buf);
	cur_renderpass = nullptr;
	return *this;
}

Pipeline const& CommandBuffer::get_bound_pipeline() const {
	return cur_pipeline;
}

CommandBuffer& CommandBuffer::bind_pipeline(std::string_view name) {
	assert(cur_renderpass && "bind_pipeline called without an active renderpass");
	Pipeline pipeline = ctx->get_or_create_pipeline(name, cur_renderpass);
	vkCmdBindPipeline(cmd_buf, static_cast<VkPipelineBindPoint>(pipeline.type), pipeline.handle);
	cur_pipeline = pipeline;
	return *this;
}

CommandBuffer& CommandBuffer::bind_compute_pipeline(std::string_view name) {
	Pipeline pipeline = ctx->get_or_create_compute_pipeline(name);
	vkCmdBindPipeline(cmd_buf, static_cast<VkPipelineBindPoint>(pipeline.type), pipeline.handle);
	cur_pipeline = pipeline;
	return *this;
}

CommandBuffer& CommandBuffer::bind_descriptor_set(VkDescriptorSet set) {
	vkCmdBindDescriptorSets(cmd_buf, static_cast<VkPipelineBindPoint>(cur_pipeline.type), cur_pipeline.layout.handle, 0, 1, &set, 0, nullptr);
	return *this;
}

CommandBuffer& CommandBuffer::bind_vertex_buffer(uint32_t first_binding, VkBuffer buffer, VkDeviceSize offset) {
	assert(cur_renderpass && "bind_vertex_buffer called without an active renderpass");
	vkCmdBindVertexBuffers(cmd_buf, first_binding, 1, &buffer, &offset);
	return *this;
}

CommandBuffer& CommandBuffer::bind_vertex_buffer(uint32_t first_binding, BufferSlice slice) {
	return bind_vertex_buffer(first_binding, slice.buffer, slice.offset);
}

CommandBuffer& CommandBuffer::auto_viewport_scissor() {
	VkViewport vp{};
	vp.width = cur_render_area.extent.width;
	vp.height = cur_render_area.extent.height;
	vp.x = 0;
	vp.y = 0;
	vp.minDepth = 0.0f;
	vp.maxDepth = 1.0f;
	vkCmdSetViewport(cmd_buf, 0, 1, &vp);
	vkCmdSetScissor(cmd_buf, 0, 1, &cur_render_area);
	return *this;
}

CommandBuffer& CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
	vkCmdDraw(cmd_buf, vertex_count, instance_count, first_vertex, first_instance);
	return *this;
}

CommandBuffer& CommandBuffer::barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkBufferMemoryBarrier const& barrier, VkDependencyFlags dependency) {
	vkCmdPipelineBarrier(cmd_buf, static_cast<VkPipelineStageFlags>(src_stage.value()), static_cast<VkPipelineStageFlags>(dst_stage.value()), dependency, 0, nullptr, 1, &barrier, 0, nullptr);
	return *this;
}

CommandBuffer& CommandBuffer::barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkImageMemoryBarrier const& barrier, VkDependencyFlags dependency) {
	vkCmdPipelineBarrier(cmd_buf, static_cast<VkPipelineStageFlags>(src_stage.value()), static_cast<VkPipelineStageFlags>(dst_stage.value()), dependency, 0, nullptr, 0, nullptr, 1, &barrier);
	return *this;
}

CommandBuffer& CommandBuffer::barrier(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::PipelineStage> dst_stage, VkMemoryBarrier const& barrier, VkDependencyFlags dependency) {
	vkCmdPipelineBarrier(cmd_buf, static_cast<VkPipelineStageFlags>(src_stage.value()), static_cast<VkPipelineStageFlags>(dst_stage.value()), dependency, 1, &barrier, 0, nullptr, 0, nullptr);
	return *this;
}

CommandBuffer& CommandBuffer::transition_layout(plib::bit_flag<ph::PipelineStage> src_stage, plib::bit_flag<ph::ResourceAccess> src_access, plib::bit_flag<ph::PipelineStage> dst_stage, plib::bit_flag<ph::ResourceAccess> dst_access,
	ph::ImageView const& view, VkImageLayout old_layout, VkImageLayout new_layout) {

	return transition_layout(src_stage, static_cast<VkAccessFlags>(src_access.value()), dst_stage, static_cast<VkAccessFlags>(dst_access.value()), view, old_layout, new_layout);
}

CommandBuffer& CommandBuffer::transition_layout(plib::bit_flag<ph::PipelineStage> src_stage, VkAccessFlags src_access, plib::bit_flag<ph::PipelineStage> dst_stage, VkAccessFlags dst_access,
	ph::ImageView const& view, VkImageLayout old_layout, VkImageLayout new_layout) {

	VkImageMemoryBarrier barrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = src_access,
		.dstAccessMask = dst_access,
		.oldLayout = old_layout,
		.newLayout = new_layout,
		.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		.image = view.image
	};
	barrier.subresourceRange = VkImageSubresourceRange{
		.aspectMask = static_cast<VkImageAspectFlags>(view.aspect),
		.baseMipLevel = view.base_level,
		.levelCount = view.level_count,
		.baseArrayLayer = view.base_layer,
		.layerCount = view.layer_count
	};
	this->barrier(src_stage, dst_stage, barrier);
	return *this;
}

CommandBuffer& CommandBuffer::dispatch(uint32_t x, uint32_t y, uint32_t z) {
	assert(cur_pipeline.type == PipelineType::Compute && "Cannot dispatch compute shader in non-compute pipeline.");
	vkCmdDispatch(cmd_buf, x, y, z);
	return *this;
}

CommandBuffer& CommandBuffer::release_ownership(Queue const& src, Queue const& dst, ph::ImageView const& view, VkImageLayout final_layout) {
	VkImageMemoryBarrier barrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = final_layout,
		.srcQueueFamilyIndex = src.family_index(),
		.dstQueueFamilyIndex = dst.family_index(),
		.image = view.image
	};
	barrier.subresourceRange = VkImageSubresourceRange{
		.aspectMask = static_cast<VkImageAspectFlags>(view.aspect),
		.baseMipLevel = view.base_level,
		.levelCount = view.level_count,
		.baseArrayLayer = view.base_layer,
		.layerCount = view.layer_count
	};
	this->barrier(ph::PipelineStage::TopOfPipe, ph::PipelineStage::BottomOfPipe, barrier);
	return *this;
}

CommandBuffer& CommandBuffer::acquire_ownership(Queue const& src, Queue const& dst, ph::ImageView const& view, VkImageLayout final_layout) {
	VkImageMemoryBarrier barrier{
		.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.pNext = nullptr,
		.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
		.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout = final_layout,
		.srcQueueFamilyIndex = src.family_index(),
		.dstQueueFamilyIndex = dst.family_index(),
		.image = view.image
	};
	barrier.subresourceRange = VkImageSubresourceRange{
		.aspectMask = static_cast<VkImageAspectFlags>(view.aspect),
		.baseMipLevel = view.base_level,
		.levelCount = view.level_count,
		.baseArrayLayer = view.base_layer,
		.layerCount = view.layer_count
	};
	this->barrier(ph::PipelineStage::TopOfPipe, ph::PipelineStage::BottomOfPipe, barrier);
	return *this;
}

VkCommandBuffer CommandBuffer::handle() const {
	return cmd_buf;
}

}