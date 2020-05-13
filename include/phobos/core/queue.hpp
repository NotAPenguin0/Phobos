#ifndef PHOBOS_QUEUE_HPP_
#define PHOBOS_QUEUE_HPP_

#include <mutex>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <phobos/forward.hpp>

namespace ph {

// Provides thread-safe access to a VkQueue
class Queue {
public:
	Queue(ph::VulkanContext& ctx, uint32_t family_index, uint32_t index);

	// Creates non-transient primary command buffers
	vk::CommandBuffer create_command_buffer();
	std::vector<vk::CommandBuffer> create_command_buffers(uint32_t count);

	vk::CommandBuffer begin_single_time(uint32_t thread = 0);
	// Submits a command buffer to the queue. Optionally a fence may be specified that is signalled when the commands are completed
	void end_single_time(vk::CommandBuffer cmd_buf, vk::Fence signal_fence = nullptr, vk::PipelineStageFlags wait_stage = {},
		vk::Semaphore wait_semaphore = nullptr, vk::Semaphore signal_semaphore = nullptr);
	void free_single_time(vk::CommandBuffer cmd_buf, uint32_t thread = 0);

	void submit(vk::CommandBuffer cmd_buf, vk::Fence signal_fence = nullptr, vk::PipelineStageFlags wait_stage = {},
		vk::Semaphore wait_semaphore = nullptr, vk::Semaphore signal_semaphore = nullptr);
	void submit(vk::SubmitInfo submit_info, vk::Fence signal_fence);
	void present(vk::PresentInfoKHR present_info);

private:
	ph::VulkanContext* ctx;

	std::mutex pool_mutex;
	vk::CommandPool command_pool;

	// As many pools as there are threads will be created.
	// Accessing a pool owned by a different thread is undefined behavior and may lead to race conditions
	std::vector<vk::CommandPool> single_time_pools;

	std::mutex queue_mutex;
	vk::Queue queue;
};

}

#endif