#include <phobos/command_buffer.hpp>
#include <phobos/context.hpp>

namespace ph {

CommandBuffer::CommandBuffer(Context& context, VkCommandBuffer&& cmd_buf) : ctx(&context), cmd_buf(cmd_buf) {

}

void CommandBuffer::begin(VkCommandBufferUsageFlags flags) {
	VkCommandBufferBeginInfo begin_info{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = flags;
	vkBeginCommandBuffer(cmd_buf, &begin_info);
}

void CommandBuffer::end() {
	vkEndCommandBuffer(cmd_buf);
}

VkCommandBuffer CommandBuffer::handle() {
	return cmd_buf;
}

}