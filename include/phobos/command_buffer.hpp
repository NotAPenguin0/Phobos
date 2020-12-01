#pragma once

#include <vulkan/vulkan.h>

namespace ph {

class Context;

class CommandBuffer {
public:
	CommandBuffer() = default;
	CommandBuffer(Context& context, VkCommandBuffer&& cmd_buf);

	void begin(VkCommandBufferUsageFlags flags = {});
	void end();

	VkCommandBuffer handle();

private:
	Context* ctx = nullptr;
	VkCommandBuffer cmd_buf = nullptr;
};

}