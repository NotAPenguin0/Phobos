#ifndef PHOBOS_IMAGE_UTIL_HPP_
#define PHOBOS_IMAGE_UTIL_HPP_

#include <vulkan/vulkan.hpp>
#include <phobos/core/vulkan_context.hpp>

#include <vk_mem_alloc.h>

#include <phobos/util/buffer_util.hpp>

namespace ph {

enum class ImageType {
    ColorAttachment,
    DepthStencilAttachment,
    // Gets vk::ImageUsageFlagBits::eSampled and vk::ImageUsageFlagBits::eTransferDst
    Texture 
};

struct RawImage {
    ImageType type{};
    vk::Format format = vk::Format::eUndefined;
    vk::Extent2D size{};
    vk::ImageLayout current_layout = vk::ImageLayout::eUndefined;
    vk::Image image = nullptr;
    VmaAllocation memory = nullptr;
};

vk::ImageView create_image_view(vk::Device device, RawImage& image, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);
RawImage create_image(VulkanContext& ctx, uint32_t width, uint32_t height, ImageType type, vk::Format format);

void transition_image_layout(vk::CommandBuffer cmd_buf, vk::Image image, vk::Format format,
    vk::ImageLayout initial_layout, vk::ImageLayout final_layout);
void transition_image_layout(vk::CommandBuffer cmd_buf, RawImage& image, vk::ImageLayout final_layout);
void copy_buffer_to_image(vk::CommandBuffer cmd_buf, RawBuffer& buffer, RawImage& image);

void destroy_image(VulkanContext& ctx, RawImage& image);

} // namespace ph

#endif