#include <phobos/util/buffer_util.hpp>
#include <phobos/util/memory_util.hpp>

namespace ph {

void create_buffer(VulkanContext& ctx, vk::DeviceSize size, vk::BufferUsageFlags flags, vk::MemoryPropertyFlags properties, 
    vk::Buffer& buffer, vk::DeviceMemory& memory) {

    vk::BufferCreateInfo info;
    info.size = size;
    info.usage = flags;
    info.sharingMode = vk::SharingMode::eExclusive;

    buffer = ctx.device.createBuffer(info);
    vk::MemoryRequirements memory_requirements = ctx.device.getBufferMemoryRequirements(buffer);
    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = memory_util::find_memory_type(ctx.physical_device, memory_requirements.memoryTypeBits, properties);
    memory = ctx.device.allocateMemory(alloc_info);
    ctx.device.bindBufferMemory(buffer, memory, 0);
}

void copy_buffer(VulkanContext& ctx, vk::Buffer src, vk::Buffer dst, vk::DeviceSize size) {
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandPool = ctx.command_pool;
    alloc_info.commandBufferCount = 1;

    vk::CommandBuffer cmd_buffer = ctx.device.allocateCommandBuffers(alloc_info)[0];

    // Record copy command buffer
    vk::CommandBufferBeginInfo begin_info;
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd_buffer.begin(begin_info);
    vk::BufferCopy copy;
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = size;
    cmd_buffer.copyBuffer(src, dst, copy);
    cmd_buffer.end();
    // Submit it
    vk::SubmitInfo submit;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd_buffer;
    ctx.graphics_queue.submit(submit, nullptr);
    ctx.device.waitIdle();

    ctx.device.freeCommandBuffers(ctx.command_pool, cmd_buffer);
}

}