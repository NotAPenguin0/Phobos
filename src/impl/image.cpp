// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Image management


#include <phobos/context.hpp>
#include <phobos/impl/context.hpp>
#include <phobos/impl/image.hpp>
#include <plib/macros.hpp>

#include <atomic>

namespace ph {
namespace impl {

static VkImageViewType get_view_type(ImageType type) {
    switch (type) {
    case ImageType::Cubemap:
    case ImageType::EnvMap:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case ImageType::Texture:
    case ImageType::ColorAttachment:
    case ImageType::DepthStencilAttachment:
    case ImageType::HdrImage:
    case ImageType::StorageImage:
        return VK_IMAGE_VIEW_TYPE_2D;
    default:
        PLIB_UNREACHABLE();
    }
}

static VkImageUsageFlags get_image_usage(ImageType type) {
    switch (type) {
    case ImageType::ColorAttachment:
        return VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    case ImageType::DepthStencilAttachment:
        return VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    case ImageType::Texture:
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    case ImageType::Cubemap:
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    case ImageType::EnvMap:
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    case ImageType::HdrImage:
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    case ImageType::StorageImage:
        return VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    default:
        PLIB_UNREACHABLE();
    }
}

static VkImageCreateFlags get_image_flags(ImageType type) {
    switch (type) {
    case ImageType::Cubemap:
    case ImageType::EnvMap:
        return VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    default: return {};
    }
}

static VkSharingMode get_sharing_mode(ImageType type) {
    switch (type) {
    case ImageType::ColorAttachment:
    case ImageType::DepthStencilAttachment:
        return VK_SHARING_MODE_EXCLUSIVE;
    default:
        return VK_SHARING_MODE_CONCURRENT;
    }
}

static VkImageTiling get_image_tiling(ImageType type) {
    return VK_IMAGE_TILING_OPTIMAL;
}

static uint64_t get_unique_image_view_id() {
    static std::atomic<uint64_t> counter = 0;
    return counter++;
}

ImageImpl::ImageImpl(ContextImpl& ctx) : ctx(&ctx) {

}

RawImage ImageImpl::create_image(ImageType type, VkExtent2D size, VkFormat format, uint32_t mips) {
    return create_image(type, size, format, VK_SAMPLE_COUNT_1_BIT, mips);
}

RawImage ImageImpl::create_image(ImageType type, VkExtent2D size, VkFormat format, VkSampleCountFlagBits samples, uint32_t mips) {
    if (type == ImageType::Cubemap || type == ImageType::EnvMap) return create_image(type, size, format, samples, mips, 6);
    else return create_image(type, size, format, samples, mips, 1);
}

RawImage ImageImpl::create_image(ImageType type, VkExtent2D size, VkFormat format, VkSampleCountFlagBits samples, uint32_t mips, uint32_t layers) {
    RawImage image;
    image.size = size;
    image.format = format;
    image.type = type;
    image.layers = layers;
    image.mip_levels = mips;
    image.samples = samples;

    VkImageCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = get_image_flags(type),
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = VkExtent3D{size.width, size.height, 1},
            .mipLevels = mips,
            .arrayLayers = image.layers,
            .samples = samples,
            .tiling = get_image_tiling(type),
            .usage = get_image_usage(type),
            .sharingMode = get_sharing_mode(type),
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    if (info.sharingMode == VK_SHARING_MODE_CONCURRENT) {
        auto const& queues = ctx->queue_family_indices();
        info.queueFamilyIndexCount = queues.size();
        info.pQueueFamilyIndices = queues.data();
    }

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    vmaCreateImage(ctx->allocator, reinterpret_cast<VkImageCreateInfo const*>(&info), &alloc_info,
                   reinterpret_cast<VkImage*>(&image.handle), &image.memory, nullptr);

    return image;
}

void ImageImpl::destroy_image(RawImage& image) {
    vmaDestroyImage(ctx->allocator, image.handle, image.memory);
    image = {};
}

ImageView ImageImpl::create_image_view(RawImage const& target, ImageAspect aspect) {
    ImageView view;

    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = target.handle;
    info.viewType = get_view_type(target.type);
    info.format = target.format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    info.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(aspect);
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = target.layers;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = target.mip_levels;

    if (target.layers > 1 && info.viewType == VK_IMAGE_VIEW_TYPE_2D) {
        info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }

    vkCreateImageView(ctx->device, &info, nullptr, &view.handle);
    view.id = get_unique_image_view_id();
    view.format = info.format;
    view.samples = target.samples;
    view.size = target.size;
    view.image = target.handle;
    view.aspect = aspect;
    view.base_layer = 0;
    view.layer_count = target.layers;
    view.base_level = 0;
    view.level_count = target.mip_levels;

    {
        std::lock_guard lock{ mutex };
        all_image_views.emplace(view.id, view);
    }

    return view;
}

ImageView ImageImpl::create_image_view(RawImage const& target, uint32_t mip, ImageAspect aspect) {
    ImageView view;

    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = target.handle;
    info.viewType = get_view_type(target.type);
    info.format = target.format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    info.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(aspect);
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = target.layers;
    info.subresourceRange.baseMipLevel = mip;
    info.subresourceRange.levelCount = 1;

    vkCreateImageView(ctx->device, &info, nullptr, &view.handle);
    view.id = get_unique_image_view_id();
    view.format = info.format;
    view.samples = target.samples;
    view.size = target.size;
    view.image = target.handle;
    view.aspect = aspect;
    view.base_layer = 0;
    view.layer_count = target.layers;
    view.base_level = mip;
    view.level_count = 1;

    {
        std::lock_guard lock{ mutex };
        all_image_views.emplace(view.id, view);
    }

    return view;
}

ImageView ImageImpl::create_image_view(RawImage const& target, uint32_t mip, uint32_t layer, ImageAspect aspect) {
    ImageView view;

    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = target.handle;
    info.viewType = get_view_type(target.type);
    info.format = target.format;
    info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    info.subresourceRange.aspectMask = static_cast<VkImageAspectFlags>(aspect);
    info.subresourceRange.baseArrayLayer = layer;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.baseMipLevel = mip;
    info.subresourceRange.levelCount = 1;

    vkCreateImageView(ctx->device, &info, nullptr, &view.handle);
    view.id = get_unique_image_view_id();
    view.format = info.format;
    view.samples = target.samples;
    view.size = target.size;
    view.image = target.handle;
    view.aspect = aspect;
    view.base_layer = layer;
    view.layer_count = 1;
    view.base_level = mip;
    view.level_count = 1;

    {
        std::lock_guard lock{ mutex };
        all_image_views.emplace(view.id, view);
    }

    return view;
}

void ImageImpl::destroy_image_view(ImageView& view) {
    vkDestroyImageView(ctx->device, view.handle, nullptr);
    {
        std::lock_guard lock{ mutex };
        all_image_views.erase(view.id);
    }
    view = ImageView{};
}

ImageView ImageImpl::get_image_view(uint64_t id) {
    std::lock_guard lock{ mutex };
    return all_image_views.at(id);
}

}
}