#include <phobos/command_buffer.hpp>
#include <phobos/context.hpp>

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
	return *this;
}

CommandBuffer& CommandBuffer::end_renderpass() {
	vkCmdEndRenderPass(cmd_buf);
	return *this;
}

VkCommandBuffer CommandBuffer::handle() {
	return cmd_buf;
}

}