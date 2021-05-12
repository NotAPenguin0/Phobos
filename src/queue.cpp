#include <phobos/queue.hpp>
#include <phobos/context.hpp>

#include <cassert>

namespace ph {

Queue::Queue(Context& context, QueueInfo info, VkQueue handle) : ctx(&context), info(info), handle(handle){
	VkCommandPoolCreateInfo pool_info{};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = info.family_index;
	// Create per-thread command pools for in-flight contexts
	in_flight_pools.resize(context.thread_count());
	pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	for (uint32_t thread_index = 0; thread_index < in_flight_pools.size(); ++thread_index) {
		vkCreateCommandPool(context.device(), &pool_info, nullptr, &in_flight_pools[thread_index]);
	}
	// Create per-frame command pools
	pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	main_pools = RingBuffer<VkCommandPool>(context.max_frames_in_flight());
	for (uint32_t frame_index = 0; frame_index < main_pools.size(); ++frame_index) {
		VkCommandPool pool = nullptr;
		vkCreateCommandPool(context.device(), &pool_info, nullptr, &pool);
		main_pools.set(frame_index, std::move(pool));
	}
}

Queue::~Queue() {
	for (VkCommandPool pool : in_flight_pools) {
		vkDestroyCommandPool(ctx->device(), pool, nullptr);
	}
	for (VkCommandPool pool : main_pools) {
		vkDestroyCommandPool(ctx->device(), pool, nullptr);
	}
}

QueueType Queue::type() const {
	return info.type;
}

bool Queue::dedicated() const {
	return info.dedicated;
}

uint32_t Queue::family_index() const {
	return info.family_index;
}

bool Queue::can_present() const {
	return info.can_present;
}

void Queue::next_frame() {
	main_pools.next();
}

void Queue::wait_idle() {
	vkQueueWaitIdle(handle);
}

CommandBuffer Queue::create_command_buffer() {
	VkCommandBufferAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandBufferCount = 1;
	info.commandPool = main_pools.current();
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VkCommandBuffer cmd_buf = nullptr;
	vkAllocateCommandBuffers(ctx->device(), &info, &cmd_buf);
	return CommandBuffer(*ctx, std::move(cmd_buf));
}

std::vector<CommandBuffer> Queue::create_command_buffers(uint32_t count) {
	VkCommandBufferAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandBufferCount = count;
	info.commandPool = main_pools.current();
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	std::vector<VkCommandBuffer> cmd_bufs(count);
	vkAllocateCommandBuffers(ctx->device(), &info, cmd_bufs.data());
	std::vector<CommandBuffer> result(count);
	for (size_t i = 0; i < count; ++i) {
		result[i] = CommandBuffer(*ctx, std::move(cmd_bufs[i]));
	}
	return result;
}

CommandBuffer Queue::begin_single_time(uint32_t thread) {
	VkCommandPool pool = in_flight_pools[thread];
	VkCommandBufferAllocateInfo info{};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	info.commandBufferCount = 1;
	info.commandPool = pool;
	info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VkCommandBuffer cmd_buf = nullptr;
	vkAllocateCommandBuffers(ctx->device(), &info, &cmd_buf);

	CommandBuffer result(*ctx, std::move(cmd_buf));
	result.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	return result;
}

void Queue::end_single_time(CommandBuffer& cmd_buf, VkFence signal_fence, VkPipelineStageFlags wait_stage, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore) {
	cmd_buf.end();
	submit(cmd_buf, signal_fence, wait_stage, wait_semaphore, signal_semaphore);
}

void Queue::free_single_time(CommandBuffer& cmd_buf, uint32_t thread) {
	VkCommandPool pool = in_flight_pools[thread];
	VkCommandBuffer cbuf = cmd_buf.handle();
	vkFreeCommandBuffers(ctx->device(), pool, 1, &cbuf);
}

void Queue::submit(CommandBuffer& cmd_buf, VkFence signal_fence, VkPipelineStageFlags wait_stage, VkSemaphore wait_semaphore, VkSemaphore signal_semaphore) {
	VkSubmitInfo info{};
	info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	info.commandBufferCount = 1;
	VkCommandBuffer cbuf = cmd_buf.handle();
	info.pCommandBuffers = &cbuf;
	if (signal_semaphore) {
		info.signalSemaphoreCount = 1;
		info.pSignalSemaphores = &signal_semaphore;
	}
	if (wait_semaphore) {
		info.waitSemaphoreCount = 1;
		info.pWaitSemaphores = &wait_semaphore;
		info.pWaitDstStageMask = &wait_stage;
	}
	submit(info, signal_fence);
}

void Queue::submit(VkSubmitInfo const& submit_info, VkFence signal_fence) {
	vkQueueSubmit(handle, 1, &submit_info, signal_fence);
}

void Queue::present(VkPresentInfoKHR const& present_info) {
	assert(can_present() && "Tried presenting from queue that has no present capabilities");
	vkQueuePresentKHR(handle, &present_info);
}

}