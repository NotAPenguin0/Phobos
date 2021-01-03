#include <phobos/image.hpp>
#include <phobos/context.hpp>

#include <atomic>

namespace ph {

VkImageViewType get_view_type(ImageType type) {
    switch (type) {
    case ImageType::Cubemap:
    case ImageType::EnvMap:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    case ImageType::Texture:
    case ImageType::ColorAttachment:
    case ImageType::DepthStencilAttachment:
    case ImageType::HdrImage:
        return VK_IMAGE_VIEW_TYPE_2D;
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

static VkImageTiling get_image_tiling(ImageType type) {
    return VK_IMAGE_TILING_OPTIMAL;
}

uint64_t get_unique_image_view_id() {
    static std::atomic<uint64_t> counter = 0;
    return counter++;
}

RawImage create_image(Context& ctx, ImageType type, VkExtent2D size, VkFormat format) {
    RawImage image;
    image.size = size;
    image.format = format;
    image.type = type;
    image.layers = 1;
    image.mip_levels = 1;
    image.samples = VK_SAMPLE_COUNT_1_BIT;

    VkImageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = get_image_flags(type),
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = VkExtent3D{size.width, size.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = get_image_tiling(type),
        .usage = get_image_usage(type),
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    vmaCreateImage(ctx.allocator, reinterpret_cast<VkImageCreateInfo const*>(&info), &alloc_info,
        reinterpret_cast<VkImage*>(&image.handle), &image.memory, nullptr);

    return image;

    return image;
}

ImageView create_image_view(Context& ctx, RawImage const& target, ImageAspect aspect) {
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

    vkCreateImageView(ctx.device, &info, nullptr, &view.handle);
    view.id = get_unique_image_view_id();
    view.format = info.format;
    view.samples = target.samples;
    view.size = target.size;

    return view;
}

void destroy_image_view(Context& ctx, ImageView& view) {
    vkDestroyImageView(ctx.device, view.handle, nullptr);
    view.handle = nullptr;
    view.id = static_cast<uint64_t>(-1);
}

bool is_depth_format(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return true;
    default:
        return false;
    }
}

} // namespace ph
