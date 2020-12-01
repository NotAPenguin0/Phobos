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

uint64_t get_unique_image_view_id() {
    static std::atomic<uint64_t> counter = 0;
    return counter++;
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

    return view;
}

void destroy_image_view(Context& ctx, ImageView& view) {
    vkDestroyImageView(ctx.device, view.handle, nullptr);
    view.handle = nullptr;
    view.id = static_cast<uint64_t>(-1);
}

} // namespace ph
