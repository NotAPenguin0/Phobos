#include <phobos/core/queue.hpp>

#include <phobos/util/memory_util.hpp>
#include <phobos/core/vulkan_context.hpp>

namespace ph {

Queue::Queue(ph::VulkanContext& ctx, uint32_t family_index, uint32_t index) : ctx(&ctx), family(family_index) {
	queue = ctx.device.getQueue(family_index, index);

	// Regular command pool for long lived command buffers
	vk::CommandPoolCreateInfo info;
	info.queueFamilyIndex = family_index;
	info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;	
	command_pool = ctx.device.createCommandPool(info);
	vk::DebugUtilsObjectNameInfoEXT cmd_pool_name_info;
	cmd_pool_name_info.objectHandle = memory_util::vk_to_u64(command_pool);
	cmd_pool_name_info.objectType = vk::ObjectType::eCommandPool;
	char buf[48];
	sprintf(buf, "Command Pool (QF: %d, I: %d)", family_index, index);
	cmd_pool_name_info.pObjectName = buf;
	ctx.device.setDebugUtilsObjectNameEXT(cmd_pool_name_info, ctx.dynamic_dispatcher);

	single_time_pools.resize(ctx.num_threads);
	// Transient command pool 
	vk::CommandPoolCreateInfo single_time_info;
	single_time_info.queueFamilyIndex = family_index;
	single_time_info.flags = vk::CommandPoolCreateFlagBits::eTransient;
	for (uint32_t tid = 0; tid < ctx.num_threads; ++tid) {
		single_time_pools[tid] = ctx.device.createCommandPool(single_time_info);
		vk::DebugUtilsObjectNameInfoEXT single_time_name_info;
		single_time_name_info.objectHandle = memory_util::vk_to_u64(single_time_pools[tid]);
		single_time_name_info.objectType = vk::ObjectType::eCommandPool;
		char transient_buf[64];
		sprintf(transient_buf, "Transient Command Pool (TID: %d, QF: %d, QI: %d)", tid, family_index, index);
		single_time_name_info.pObjectName = transient_buf;
		ctx.device.setDebugUtilsObjectNameEXT(single_time_name_info, ctx.dynamic_dispatcher);
	}
}

vk::CommandBuffer Queue::create_command_buffer() {
	vk::CommandBufferAllocateInfo info;
	info.commandBufferCount = 1;
	info.commandPool = command_pool;
	info.level = vk::CommandBufferLevel::ePrimary;
	std::lock_guard lock(pool_mutex);
	return ctx->device.allocateCommandBuffers(info)[0];
}

std::vector<vk::CommandBuffer> Queue::create_command_buffers(uint32_t count) {
	vk::CommandBufferAllocateInfo info;
	info.commandBufferCount = count;
	info.commandPool = command_pool;
	info.level = vk::CommandBufferLevel::ePrimary;
	std::lock_guard lock(pool_mutex);
	return ctx->device.allocateCommandBuffers(info);
}

vk::CommandBuffer Queue::begin_single_time(uint32_t thread) {
	vk::CommandBufferAllocateInfo info;
	info.commandBufferCount = 1;
	info.commandPool = single_time_pools[thread];
	info.level = vk::CommandBufferLevel::ePrimary;

	vk::CommandBuffer cmd_buf = ctx->device.allocateCommandBuffers(info)[0];

	vk::CommandBufferBeginInfo begin_info;
	begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	cmd_buf.begin(begin_info);

	return cmd_buf;
}

void Queue::end_single_time(vk::CommandBuffer cmd_buf, vk::Fence signal_fence, vk::PipelineStageFlags wait_stage, vk::Semaphore wait_semaphore,
	vk::Semaphore signal_semaphore) {
	cmd_buf.end();

	submit(cmd_buf, signal_fence, wait_stage, wait_semaphore, signal_semaphore);
}

void Queue::submit(vk::CommandBuffer cmd_buf, vk::Fence signal_fence, vk::PipelineStageFlags wait_stage, vk::Semaphore wait_semaphore, 
	vk::Semaphore signal_semaphore) {
	vk::SubmitInfo submit_info;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &cmd_buf;
	if (wait_semaphore) {
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &wait_semaphore;
		submit_info.pWaitDstStageMask = &wait_stage;
	}

	if (signal_semaphore) {
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &signal_semaphore;
	}

	submit(submit_info, signal_fence);
}

void Queue::submit(vk::SubmitInfo submit_info, vk::Fence signal_fence) {
	std::lock_guard lock(queue_mutex);
	queue.submit(submit_info, signal_fence);
}

void Queue::present(vk::PresentInfoKHR present_info) {
	std::lock_guard lock(queue_mutex);
	queue.presentKHR(present_info);
}

void Queue::free_single_time(vk::CommandBuffer cmd_buf, uint32_t thread) {
	ctx->device.freeCommandBuffers(single_time_pools[thread], cmd_buf);
}

void Queue::release_ownership(vk::CommandBuffer cmd_buf, ph::RawImage& image, Queue& dst) {
	vk::ImageMemoryBarrier barrier;
	barrier.srcQueueFamilyIndex = family;
	barrier.dstQueueFamilyIndex = dst.family;
	barrier.image = image.image;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = image.layers;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.oldLayout = image.current_layout;
	barrier.newLayout = image.current_layout;
	cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, 
		vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);
}

void Queue::release_ownership(vk::CommandBuffer cmd_buf, ph::RawBuffer& buffer, Queue& dst) {
	vk::BufferMemoryBarrier barrier;
	barrier.srcQueueFamilyIndex = family;
	barrier.dstQueueFamilyIndex = dst.family;
	barrier.buffer = buffer.buffer;
	barrier.offset = 0;
	barrier.size = buffer.size;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput,
		vk::DependencyFlagBits::eByRegion, nullptr, barrier, nullptr);
}


void Queue::acquire_ownership(vk::CommandBuffer cmd_buf, ph::RawImage& image, Queue& src) {
	vk::ImageMemoryBarrier barrier;
	barrier.srcQueueFamilyIndex = src.family;
	barrier.dstQueueFamilyIndex = family;
	barrier.image = image.image;
	barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = image.layers;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	barrier.oldLayout = image.current_layout;
	barrier.newLayout = image.current_layout;
	cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
		vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);
}

void Queue::acquire_ownership(vk::CommandBuffer cmd_buf, ph::RawBuffer& buffer, Queue& src) {
	vk::BufferMemoryBarrier barrier;
	barrier.srcQueueFamilyIndex = src.family;
	barrier.dstQueueFamilyIndex = family;
	barrier.buffer = buffer.buffer;
	barrier.offset = 0;
	barrier.size = buffer.size;
	barrier.dstAccessMask = vk::AccessFlagBits::eVertexAttributeRead;
	cmd_buf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eVertexInput,
		vk::DependencyFlagBits::eByRegion, nullptr, barrier, nullptr);
}

}