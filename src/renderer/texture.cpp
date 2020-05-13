#include <phobos/renderer/texture.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/util/buffer_util.hpp>
#include <phobos/core/vulkan_context.hpp>

namespace ph {

Texture::Texture(VulkanContext& ctx) : ctx(&ctx) {}

Texture::Texture(CreateInfo const& info) : Texture(*info.ctx) {
    create(info);
}

Texture::Texture(Texture&& rhs) : Texture(*rhs.ctx) {
    std::swap(image, rhs.image);
    std::swap(view, rhs.view);
}

Texture& Texture::operator=(Texture&& rhs) {
    if (this != &rhs) {
        ctx = rhs.ctx;
        std::swap(image, rhs.image);
        std::swap(view, rhs.view);
    }
    return *this;
}

Texture::~Texture() {
    destroy();
}

void Texture::create(CreateInfo const& info) {
    destroy();

    vk::DeviceSize const size = info.width * info.height * info.channels;
    // Create staging buffer
    RawBuffer staging_buffer = create_buffer(*ctx, size, BufferType::TransferBuffer);
    
    std::byte* data_ptr = map_memory(*ctx, staging_buffer);
    std::memcpy(data_ptr, info.data, size);
    flush_memory(*ctx, staging_buffer, 0, VK_WHOLE_SIZE);
    unmap_memory(*ctx, staging_buffer);

    image = create_image(*ctx, info.width, info.height, ImageType::Texture, info.format);
    
    vk::CommandBuffer cmd_buf = ctx->graphics->begin_single_time();
    // Transition image layout to TransferDst so we can start fillig the image with data
    transition_image_layout(cmd_buf, image, vk::ImageLayout::eTransferDstOptimal);
    copy_buffer_to_image(cmd_buf, whole_buffer_slice(*ctx, staging_buffer), image);
    // Transition image layout to ShaderReadOnlyOptimal so we can start sampling from it
    transition_image_layout(cmd_buf, image, vk::ImageLayout::eShaderReadOnlyOptimal);
    ctx->graphics->end_single_time(cmd_buf);
    ctx->device.waitIdle();

    destroy_buffer(*ctx, staging_buffer);

    view = create_image_view(ctx->device, image);
}

void Texture::destroy() {
    if (image.image) {
        destroy_image_view(*ctx, view);
        destroy_image(*ctx, image);
    }
}

vk::Image Texture::handle() const {
    return image.image;
}

ImageView Texture::view_handle() const {
    return view;
}

VmaAllocation Texture::memory_handle() const {
    return image.memory;
}

} // namespace ph