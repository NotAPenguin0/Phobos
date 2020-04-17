#include <phobos/util/image_util.hpp>
#include <phobos/util/memory_util.hpp>

#include <atomic>

namespace ph {

static vk::ImageUsageFlags get_image_usage(ImageType type) {
    switch (type) {
    case ImageType::ColorAttachment:
        return vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    case ImageType::DepthStencilAttachment:
        return vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
    case ImageType::Texture: 
        return vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    }
}

stl::uint64_t get_unique_image_view_id() {
    static std::atomic<stl::uint64_t> counter = 0;
    return counter++;
}

ImageView create_image_view(vk::Device device, RawImage& image, vk::ImageAspectFlags aspect) {
    ImageView view;

    vk::ImageViewCreateInfo info;
    info.image = image.image;
    info.viewType = vk::ImageViewType::e2D;
    info.format = image.format;
    info.components.r = vk::ComponentSwizzle::eIdentity;
    info.components.g = vk::ComponentSwizzle::eIdentity;
    info.components.b = vk::ComponentSwizzle::eIdentity;
    info.components.a = vk::ComponentSwizzle::eIdentity;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = 1;

    view.view = device.createImageView(info);
    view.id = get_unique_image_view_id();

    return view;
}

void destroy_image_view(VulkanContext& ctx, ImageView& view) {
    ctx.device.destroyImageView(view.view);
    view.view = nullptr;
    view.id = static_cast<stl::uint64_t>(-1);
}

RawImage create_image(VulkanContext& ctx, uint32_t width, uint32_t height, ImageType type, vk::Format format) {
    RawImage image;
    image.format = format;
    image.size = vk::Extent2D{ width, height };
    image.type = type;

    vk::ImageCreateInfo info;
    info.imageType = vk::ImageType::e2D;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = 1;
    info.arrayLayers = 1;

    info.format = format;
    info.tiling = vk::ImageTiling::eOptimal;
    info.initialLayout = vk::ImageLayout::eUndefined;
    info.usage = get_image_usage(type);
    info.samples = vk::SampleCountFlagBits::e1;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    vmaCreateImage(ctx.allocator, reinterpret_cast<VkImageCreateInfo const*>(&info), &alloc_info, 
        reinterpret_cast<VkImage*>(&image.image), &image.memory, nullptr);

    return image;
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
    } else if (initial_layout == vk::ImageLayout::ePresentSrcKHR && final_layout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else if (initial_layout == vk::ImageLayout::eColorAttachmentOptimal && final_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    } else {
        throw std::invalid_argument("Unsupported image layout transition");
    }

    cmd_buf.pipelineBarrier(src_stage, dst_stage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);
}

void transition_image_layout(vk::CommandBuffer cmd_buf, RawImage& image, vk::ImageLayout final_layout) {
    vk::ImageMemoryBarrier barrier;
    barrier.oldLayout = image.current_layout;
    barrier.newLayout = final_layout;
    // We don't want to transfer queue family ownership
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    barrier.image = image.image;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    vk::PipelineStageFlags src_stage;
    vk::PipelineStageFlags dst_stage;

    // Figure out source and desitionation stage based on image layouts
    if (image.current_layout == vk::ImageLayout::eUndefined && final_layout == vk::ImageLayout::eTransferDstOptimal) {
        barrier.srcAccessMask = vk::AccessFlags{};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    }
    else if (image.current_layout == vk::ImageLayout::eTransferDstOptimal && final_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else if (image.current_layout == vk::ImageLayout::ePresentSrcKHR && final_layout == vk::ImageLayout::eColorAttachmentOptimal) {
        barrier.srcAccessMask = {};
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else if (image.current_layout == vk::ImageLayout::eColorAttachmentOptimal && final_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        src_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else {
        throw std::invalid_argument("Unsupported image layout transition");
    }

    cmd_buf.pipelineBarrier(src_stage, dst_stage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);
    // Set correct image layout
    image.current_layout = final_layout;
}

void copy_buffer_to_image(vk::CommandBuffer cmd_buf, RawBuffer& buffer, RawImage& image) {

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
    copy_region.imageExtent = vk::Extent3D{image.size.width, image.size.height, 1};

    cmd_buf.copyBufferToImage(buffer.buffer, image.image, image.current_layout, copy_region);
}

void destroy_image(VulkanContext& ctx, RawImage& image) {
    vmaDestroyImage(ctx.allocator, image.image, image.memory);
    image.image = nullptr;
    image.memory = nullptr;
    image.size = vk::Extent2D{0, 0};
    image.type = {};
    image.current_layout = vk::ImageLayout::eUndefined;
    image.format = vk::Format::eUndefined;
}

}