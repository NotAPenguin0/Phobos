#include <phobos/util/cmdbuf_util.hpp>

namespace ph {

vk::CommandBuffer begin_single_time_command_buffer(VulkanContext& ctx) {
    vk::CommandBufferAllocateInfo info;
    info.level = vk::CommandBufferLevel::ePrimary;
    info.commandBufferCount = 1;
    info.commandPool = ctx.command_pool;

    vk::CommandBuffer buffer = ctx.device.allocateCommandBuffers(info)[0];

    vk::CommandBufferBeginInfo begin_info;
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    buffer.begin(begin_info);

    return buffer;
}

void end_single_time_command_buffer(VulkanContext& ctx, vk::CommandBuffer buffer) {
    buffer.end();

    vk::SubmitInfo info;
    info.commandBufferCount = 1;
    info.pCommandBuffers = &buffer;
    
    ctx.graphics_queue.submit(info, nullptr);
    ctx.graphics_queue.waitIdle();

    ctx.device.freeCommandBuffers(ctx.command_pool, buffer);
}

}