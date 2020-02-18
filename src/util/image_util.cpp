#include <phobos/util/image_util.hpp>

namespace ph {

vk::ImageView create_image_view(vk::Device device, vk::Image image, vk::Format format, vk::ImageAspectFlags aspect) {
    vk::ImageViewCreateInfo info;
    info.image = image;
    info.viewType = vk::ImageViewType::e2D;
    info.format = format;
    info.components.r = vk::ComponentSwizzle::eIdentity;
    info.components.g = vk::ComponentSwizzle::eIdentity;
    info.components.b = vk::ComponentSwizzle::eIdentity;
    info.components.a = vk::ComponentSwizzle::eIdentity;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;

    return device.createImageView(info);
}

}