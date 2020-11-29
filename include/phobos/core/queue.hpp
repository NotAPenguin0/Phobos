#pragma once

#include <vulkan/vulkan.h>

namespace ph {

enum class QueueType {
	Graphics = VK_QUEUE_GRAPHICS_BIT,
	Compute = VK_QUEUE_COMPUTE_BIT,
	Transfer = VK_QUEUE_TRANSFER_BIT
};

struct QueueRequest {
	// This flag is a preference. If no dedicated queue was found, a shared one will be selected.
	bool dedicated = false;
	QueueType type{};
};

struct QueueInfo {
	bool dedicated = false;
	QueueType type{};
	uint32_t family_index = 0;
};

class Queue {
public:
	Queue(QueueInfo info, VkQueue handle);

	QueueType type() const;
	bool dedicated() const;
	uint32_t family_index() const;

private:
	QueueInfo info{};
	VkQueue handle = nullptr;
};

}