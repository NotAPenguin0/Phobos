#pragma once

#include <phobos/command_buffer.hpp>
#include <phobos/ring_buffer.hpp>

#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>

namespace ph {

class Context;

enum class QueueType {
	Graphics = VK_QUEUE_GRAPHICS_BIT,
	Compute = VK_QUEUE_COMPUTE_BIT,
	Transfer = VK_QUEUE_TRANSFER_BIT
};

std::string_view to_string(QueueType type);

struct QueueRequest {
	// This flag is a preference. If no dedicated queue was found, a shared one will be selected.
	bool dedicated = false;
	QueueType type{};
};

struct QueueInfo {
	bool dedicated = false;
	bool can_present = false;
	QueueType type{};
	uint32_t family_index = 0;
};

class Queue {
public:
	Queue(Context& context, QueueInfo info, VkQueue handle);
	~Queue();

	Queue(Queue const&) = delete;
	Queue(Queue&& rhs) = default;

	Queue& operator=(Queue const&) = delete;
	Queue& operator=(Queue&& rhs) = default;

	QueueType type() const;
	bool dedicated() const;
	uint32_t family_index() const;
	bool can_present() const;

	void next_frame();
	void wait_idle();

	// Creates non-transient primary command buffers
	CommandBuffer create_command_buffer();
	std::vector<CommandBuffer> create_command_buffers(uint32_t count);

	// Creates a single-time command buffer. It may only be used on the same thread as it was created on.
	CommandBuffer begin_single_time(uint32_t thread);
	// Submits a command buffer to the queue. Optionally a fence may be specified that is signalled when the commands are completed
	void end_single_time(CommandBuffer& cmd_buf, VkFence signal_fence = nullptr, VkPipelineStageFlags wait_stage = {},
		VkSemaphore wait_semaphore = nullptr, VkSemaphore signal_semaphore = nullptr);
	void free_single_time(CommandBuffer& cmd_buf, uint32_t thread);

	void submit(CommandBuffer& cmd_buf, VkFence signal_fence = nullptr, VkPipelineStageFlags wait_stage = {},
		VkSemaphore wait_semaphore = nullptr, VkSemaphore signal_semaphore = nullptr);
	void submit(VkSubmitInfo const& submit_info, VkFence signal_fence);
	void present(VkPresentInfoKHR const& present_info);

	// Releases onwnership of a resource. This must be externally synchronized with acquire_ownership in the other queue
/*	void release_ownership(CommandBuffer& cmd_buf, ph::RawImage& image, Queue& dst);
	void release_ownership(CommandBuffer& cmd_buf, ph::RawBuffer& buffer, Queue& dst);
	// Acquire ownership of a resource.
	void acquire_ownership(CommandBuffer& cmd_buf, ph::RawImage& image, Queue& src);
	void acquire_ownership(CommandBuffer& cmd_buf, ph::RawBuffer& buffer, Queue& src);
*/

private:
	Context* ctx = nullptr;
	QueueInfo info{};
	VkQueue handle = nullptr;

	// Per frame
	RingBuffer<VkCommandPool> main_pools;
	// Per thread
	std::vector<VkCommandPool> in_flight_pools;

	std::unique_ptr<std::mutex> mutex;
};

}