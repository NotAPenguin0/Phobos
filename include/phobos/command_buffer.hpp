#pragma once

#include <vulkan/vulkan.h>

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

	VkCommandBuffer handle();

private:
	Context* ctx = nullptr;
	VkCommandBuffer cmd_buf = nullptr;
};

}