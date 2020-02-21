#ifndef PHOBOS_IMAGE_UTIL_HPP_
#define PHOBOS_IMAGE_UTIL_HPP_

#include <vulkan/vulkan.hpp>
#include <phobos/core/vulkan_context.hpp>

namespace ph {

vk::ImageView create_image_view(vk::Device device, vk::Image image, vk::Format format, 
    vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);

void create_image(VulkanContext& ctx, size_t width, size_t height, vk::Format format, vk::ImageTiling tiling,
     vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& memory);

void transition_image_layout(vk::CommandBuffer cmd_buf, vk::Image image, vk::Format format, 
    vk::ImageLayout initial_layout, vk::ImageLayout final_layout);

void copy_buffer_to_image(vk::CommandBuffer cmd_buf, vk::Buffer buffer, vk::Image image,
    vk::ImageLayout image_layout, size_t width, size_t height);

} // namespace ph

#endif