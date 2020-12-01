#pragma once

#include <vulkan/vulkan.h>

namespace ph {

class Context;

enum class ImageType {
    ColorAttachment,
    DepthStencilAttachment,
    Texture,
    Cubemap,
    EnvMap,
    HdrImage
};

enum class ImageAspect {
    Color = VK_IMAGE_ASPECT_COLOR_BIT,
    Depth = VK_IMAGE_ASPECT_DEPTH_BIT
};

struct RawImage {
    ImageType type{};
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkExtent2D size{};
    uint32_t layers = 1;
    uint32_t mip_levels = 1;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout current_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImage handle = nullptr;
    VkDeviceMemory memory = nullptr;
};

struct ImageView {
    VkImageView handle = nullptr;

    // Id that is guaranteed to be unique for each VkImageView.
    // We need this because Vulkan doesn't guarantee unique id's for vk handles.
    uint64_t id = static_cast<uint64_t>(-1);

    inline bool operator==(ImageView const& rhs) const {
        return id == rhs.id;
    }
};

ImageView create_image_view(Context& ctx, RawImage const& target, ImageAspect aspect = ImageAspect::Color);

void destroy_image_view(Context& ctx, ImageView& view);

}