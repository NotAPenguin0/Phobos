#include <phobos/renderer/texture.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/util/buffer_util.hpp>
#include <phobos/util/cmdbuf_util.hpp>

namespace ph {

Texture::Texture(VulkanContext& ctx) : ctx(&ctx) {}

Texture::Texture(CreateInfo const& info) : Texture(*info.ctx) {
    create(info);
}

Texture::Texture(Texture&& rhs) : Texture(*rhs.ctx) {
    std::swap(image, rhs.image);
    std::swap(view, rhs.view);
    std::swap(memory, rhs.memory);
}

Texture& Texture::operator=(Texture&& rhs) {
    if (this != &rhs) {
        ctx = rhs.ctx;
        std::swap(image, rhs.image);
        std::swap(view, rhs.view);
        std::swap(memory, rhs.memory);
    }
    return *this;
}

Texture::~Texture() {
    destroy();
}

void Texture::create(CreateInfo const& info) {
    destroy();
    
    vk::Buffer staging_buffer;
    vk::DeviceMemory staging_buffer_memory;

    vk::DeviceSize size = info.width * info.height * info.channels;
    create_buffer(*ctx, size, vk::BufferUsageFlagBits::eTransferSrc, 
        vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible, 
        staging_buffer, staging_buffer_memory);
    void* data_ptr = ctx->device.mapMemory(staging_buffer_memory, 0, size);
    std::memcpy(data_ptr, info.data, size);
    ctx->device.unmapMemory(staging_buffer_memory);

    create_image(*ctx, info.width, info.height, info.format, vk::ImageTiling::eOptimal, 
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, 
        vk::MemoryPropertyFlagBits::eDeviceLocal, image, memory);
    
    vk::CommandBuffer cmd_buf = begin_single_time_command_buffer(*ctx);
    // Transition image layout to TransferDst so we can start fillig the image with data
    transition_image_layout(cmd_buf, image, info.format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
    copy_buffer_to_image(cmd_buf, staging_buffer, image, vk::ImageLayout::eTransferDstOptimal, info.width, info.height);
    // Transition image layout to ShaderReadOnlyOptimal so we can start sampling from it
    transition_image_layout(cmd_buf, image, info.format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    end_single_time_command_buffer(*ctx, cmd_buf);

    ctx->device.destroyBuffer(staging_buffer);
    ctx->device.freeMemory(staging_buffer_memory);

    view = create_image_view(ctx->device, image, info.format);
}

void Texture::destroy() {
    if (image) {
        ctx->device.destroyImage(image);
        image = nullptr;
    }
    if (view) {
        ctx->device.destroyImageView(view);
        view = nullptr;
    }
    if (memory) {
        ctx->device.freeMemory(memory);
        memory = nullptr;
    }
}

vk::Image Texture::handle() {
    return image;
}

vk::ImageView Texture::view_handle() {
    return view;
}

vk::DeviceMemory Texture::memory_handle() {
    return memory;
}

} // namespace ph