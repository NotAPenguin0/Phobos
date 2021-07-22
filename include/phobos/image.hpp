#pragma once

#include <vulkan/vulkan.h>
#include "vk_mem_alloc.h"

namespace ph {

class Context;

enum class ImageType {
    ColorAttachment,
    DepthStencilAttachment,
    Texture,
    Cubemap,
    EnvMap,
    HdrImage,
    StorageImage
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
    VmaAllocation memory = nullptr;
};

struct ImageView {
    VkImage image = nullptr;
    VkImageView handle = nullptr;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageAspect aspect = {};
    VkExtent2D size{};
    uint32_t base_level = 0;
    uint32_t level_count = 0;
    uint32_t base_layer = 0;
    uint32_t layer_count = 0;

    // Id that is guaranteed to be unique for each VkImageView.
    // We need this because Vulkan doesn't guarantee unique id's for vk handles.
    uint64_t id = static_cast<uint64_t>(-1);

    inline bool operator==(ImageView const& rhs) const {
        return id == rhs.id;
    }
};

// Common utilities

bool is_depth_format(VkFormat format);

}