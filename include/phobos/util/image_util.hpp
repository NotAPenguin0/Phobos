#ifndef PHOBOS_IMAGE_UTIL_HPP_
#define PHOBOS_IMAGE_UTIL_HPP_

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>

#include <phobos/util/buffer_util.hpp>

#include <stl/types.hpp>
#include <stl/span.hpp>

namespace ph {

struct VulkanContext;

enum class ImageType {
    ColorAttachment,
    DepthStencilAttachment,
    // Gets vk::ImageUsageFlagBits::eSampled and vk::ImageUsageFlagBits::eTransferDst
    Texture,
    Cubemap,
    EnvMap,
    HdrImage
};

struct RawImage {
    ImageType type{};
    vk::Format format = vk::Format::eUndefined;
    vk::Extent2D size{};
    uint32_t layers = 1;
    vk::ImageLayout current_layout = vk::ImageLayout::eUndefined;
    vk::Image image = nullptr;
    VmaAllocation memory = nullptr;
};

struct ImageView {
    vk::ImageView view = nullptr;
    // Id that is guaranteed to be unique for each VkImageView.
    // We need this because Vulkan doesn't guarantee unique id's for vk handles.
    stl::uint64_t id = static_cast<stl::uint64_t>(-1);

    bool operator==(ImageView const& rhs) const {
        return id == rhs.id;
    }
};

ImageView create_image_view(vk::Device device, RawImage& image, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);
ImageView create_image_view(vk::Device device, RawImage& image, uint32_t layer, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);
void destroy_image_view(VulkanContext& ctx, ImageView& view);

// this function is thread safe
stl::uint64_t get_unique_image_view_id();

RawImage create_image(VulkanContext& ctx, uint32_t width, uint32_t height, ImageType type, vk::Format format, uint32_t layers = 1);

void transition_image_layout(vk::CommandBuffer cmd_buf, vk::Image image, vk::Format format,
    vk::ImageLayout initial_layout, vk::ImageLayout final_layout);
void transition_image_layout(vk::CommandBuffer cmd_buf, RawImage& image, vk::ImageLayout final_layout);

void copy_buffer_to_image(vk::CommandBuffer cmd_buf, BufferSlice slice, RawImage& image);
void copy_buffer_to_image(vk::CommandBuffer cmd_buf, stl::span<BufferSlice> slices, RawImage& image);

void destroy_image(VulkanContext& ctx, RawImage& image);

} // namespace ph

#endif