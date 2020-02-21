#include <phobos/util/image_util.hpp>
#include <phobos/util/memory_util.hpp>

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

void create_image(VulkanContext& ctx, size_t width, size_t height, vk::Format format, vk::ImageTiling tiling,
     vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& memory) {

    vk::ImageCreateInfo info;
    info.imageType = vk::ImageType::e2D;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;

    info.format = format;
    info.tiling = tiling;
    info.initialLayout = vk::ImageLayout::eUndefined;
    info.usage = usage;
    info.samples = vk::SampleCountFlagBits::e1;

    image = ctx.device.createImage(info);

    // Allocate memory for the image
    vk::MemoryRequirements const mem_requirements = ctx.device.getImageMemoryRequirements(image);
    vk::MemoryAllocateInfo alloc_info;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = memory_util::find_memory_type(ctx.physical_device, mem_requirements.memoryTypeBits, properties);
    memory = ctx.device.allocateMemory(alloc_info);
    ctx.device.bindImageMemory(image, memory, 0);
}

void transition_image_layout(vk::CommandBuffer cmd_buf, vk::Image image, vk::Format format, 
    vk::ImageLayout initial_layout, vk::ImageLayout final_layout) {

    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = initial_layout;
    barrier.newLayout = final_layout;
    // We don't want to transfer queue family ownership
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    
    barrier.image = image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;

    // Figure out source and desitionation stage based on image layouts
    if (initial_layout == vk::ImageLayout::eUndefined && final_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlags {};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (initial_layout == vk::ImageLayout::eTransferDstOptimal && final_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("Unsupported image layout transition");
    }

    cmd_buf.pipelineBarrier(src_stage, dst_stage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);
}

void copy_buffer_to_image(vk::CommandBuffer cmd_buf, vk::Buffer buffer, vk::Image image,
    vk::ImageLayout image_layout, size_t width, size_t height) {

    vk::BufferImageCopy copy_region;
    copy_region.bufferOffset = 0;
    // Values of 0 means tightly packed here
    copy_region.bufferRowLength = 0;
    copy_region.bufferImageHeight = 0;

    copy_region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = 1;
    copy_region.imageSubresource.mipLevel = 0;
    
    copy_region.imageOffset = vk::Offset3D{0, 0, 0};
    copy_region.imageExtent = vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

    cmd_buf.copyBufferToImage(buffer, image, image_layout, copy_region);
}

}