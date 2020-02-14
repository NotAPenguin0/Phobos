#ifndef PHOBOS_BUFFER_UTIL_HPP_
#define PHOBOS_BUFFER_UTIL_HPP_

#include <phobos/core/vulkan_context.hpp>

namespace ph {

void create_buffer(VulkanContext& ctx, vk::DeviceSize size, vk::BufferUsageFlags flags, vk::MemoryPropertyFlags properties, 
    vk::Buffer& buffer, vk::DeviceMemory& memory);

void copy_buffer(VulkanContext& ctx, vk::Buffer src, vk::Buffer dst, vk::DeviceSize size);

}

#endif