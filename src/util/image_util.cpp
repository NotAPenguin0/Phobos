#include <phobos/util/image_util.hpp>
#include <phobos/util/memory_util.hpp>

#include <atomic>

#include <stl/enumerate.hpp>

namespace ph {

static vk::ImageUsageFlags get_image_usage(ImageType type) {
    switch (type) {
    case ImageType::ColorAttachment:
        return vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    case ImageType::DepthStencilAttachment:
        return vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eSampled;
    case ImageType::Texture: 
        return vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eSampled;
    case ImageType::Cubemap:
        return vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc | 
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment;
    case ImageType::EnvMap:
        return vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eTransferSrc |
            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
    case ImageType::HdrImage:
        return vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage;
    }
}

static vk::ImageCreateFlags get_image_flags(ImageType type) {
    switch (type) {
    case ImageType::Cubemap:
    case ImageType::EnvMap:
        return vk::ImageCreateFlagBits::eCubeCompatible;
    default: return {};
    }
}

static vk::ImageTiling get_image_tiling(ImageType type) {
    auto usage = get_image_usage(type);
    return vk::ImageTiling::eOptimal;
}

stl::uint64_t get_unique_image_view_id() {
    static std::atomic<stl::uint64_t> counter = 0;
    return counter++;
}

vk::ImageViewType get_view_type(ImageType type) {
    switch (type) {
    case ImageType::Cubemap:
    case ImageType::EnvMap:
        return vk::ImageViewType::eCube;
    case ImageType::Texture:
    case ImageType::ColorAttachment:
    case ImageType::DepthStencilAttachment:
    case ImageType::HdrImage:
        return vk::ImageViewType::e2D;
    }
}

ImageView create_image_view(vk::Device device, RawImage& image, vk::ImageAspectFlags aspect) {
    ImageView view;

    vk::ImageViewCreateInfo info;
    info.image = image.image;
    info.viewType = get_view_type(image.type);
    info.format = image.format;
    info.components.r = vk::ComponentSwizzle::eIdentity;
    info.components.g = vk::ComponentSwizzle::eIdentity;
    info.components.b = vk::ComponentSwizzle::eIdentity;
    info.components.a = vk::ComponentSwizzle::eIdentity;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = image.layers;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = image.mip_levels;

    view.view = device.createImageView(info);
    view.id = get_unique_image_view_id();

    return view;
}

ImageView create_image_view(vk::Device device, RawImage& image, uint32_t layer, vk::ImageAspectFlags aspect) {
    ImageView view;

    vk::ImageViewCreateInfo info;
    info.image = image.image;
    info.viewType = get_view_type(image.type);
    info.format = image.format;
    info.components.r = vk::ComponentSwizzle::eIdentity;
    info.components.g = vk::ComponentSwizzle::eIdentity;
    info.components.b = vk::ComponentSwizzle::eIdentity;
    info.components.a = vk::ComponentSwizzle::eIdentity;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseArrayLayer = layer;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.baseMipLevel = 0;
    info.subresourceRange.levelCount = image.mip_levels;

    view.view = device.createImageView(info);
    view.id = get_unique_image_view_id();

    return view;
}

ImageView create_image_view(vk::Device device, RawImage& image, uint32_t layer, uint32_t mip_level, vk::ImageAspectFlags aspect) {
    ImageView view;

    vk::ImageViewCreateInfo info;
    info.image = image.image;
    info.viewType = get_view_type(image.type);
    info.format = image.format;
    info.components.r = vk::ComponentSwizzle::eIdentity;
    info.components.g = vk::ComponentSwizzle::eIdentity;
    info.components.b = vk::ComponentSwizzle::eIdentity;
    info.components.a = vk::ComponentSwizzle::eIdentity;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseArrayLayer = layer;
    info.subresourceRange.layerCount = 1;
    info.subresourceRange.baseMipLevel = mip_level;
    info.subresourceRange.levelCount = 1;

    view.view = device.createImageView(info);
    view.id = get_unique_image_view_id();

    return view;
}

ImageView create_image_view_level(vk::Device device, RawImage& image, uint32_t mip_level, vk::ImageAspectFlags aspect) {
    ImageView view;

    vk::ImageViewCreateInfo info;
    info.image = image.image;
    info.viewType = get_view_type(image.type);
    info.format = image.format;
    info.components.r = vk::ComponentSwizzle::eIdentity;
    info.components.g = vk::ComponentSwizzle::eIdentity;
    info.components.b = vk::ComponentSwizzle::eIdentity;
    info.components.a = vk::ComponentSwizzle::eIdentity;
    info.subresourceRange.aspectMask = aspect;
    info.subresourceRange.baseArrayLayer = 0;
    info.subresourceRange.layerCount = image.layers;
    info.subresourceRange.baseMipLevel = mip_level;
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

RawImage create_image(VulkanContext& ctx, uint32_t width, uint32_t height, ImageType type, vk::Format format, uint32_t layers, uint32_t mip_levels) {
    RawImage image;
    image.format = format;
    image.size = vk::Extent2D{ width, height };
    image.type = type;
    image.layers = layers;
    image.mip_levels = mip_levels;

    vk::ImageCreateInfo info;
    info.imageType = vk::ImageType::e2D;
    info.extent.width = width;
    info.extent.height = height;
    info.extent.depth = 1;
    info.mipLevels = mip_levels;
    info.arrayLayers = layers;
    info.sharingMode = vk::SharingMode::eExclusive;

    info.format = format;
    info.tiling = get_image_tiling(type);
    info.initialLayout = vk::ImageLayout::eUndefined;
    info.usage = get_image_usage(type);
    info.samples = vk::SampleCountFlagBits::e1;
    info.flags = get_image_flags(type);

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
    barrier.subresourceRange.layerCount = image.layers;
    barrier.subresourceRange.levelCount = image.mip_levels;

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
    else if (image.current_layout == vk::ImageLayout::eUndefined && final_layout == vk::ImageLayout::eGeneral) {
        barrier.srcAccessMask = vk::AccessFlags{};
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        src_stage = vk::PipelineStageFlagBits::eTopOfPipe;
        dst_stage = vk::PipelineStageFlagBits::eComputeShader;
    }
    else if (image.current_layout == vk::ImageLayout::eTransferDstOptimal && final_layout == vk::ImageLayout::eGeneral) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eComputeShader;
    }
    else if (image.current_layout == vk::ImageLayout::eGeneral && final_layout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        src_stage = vk::PipelineStageFlagBits::eComputeShader;
        dst_stage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    else if (image.current_layout == vk::ImageLayout::eGeneral && final_layout == vk::ImageLayout::eTransferSrcOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

        src_stage = vk::PipelineStageFlagBits::eComputeShader;
        dst_stage = vk::PipelineStageFlagBits::eTransfer;
    } else if (image.current_layout == vk::ImageLayout::eTransferSrcOptimal && final_layout == vk::ImageLayout::eGeneral) {
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;

        src_stage = vk::PipelineStageFlagBits::eTransfer;
        dst_stage = vk::PipelineStageFlagBits::eComputeShader;
    }
    else {
        throw std::invalid_argument("Unsupported image layout transition");
    }

    cmd_buf.pipelineBarrier(src_stage, dst_stage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);
    // Set correct image layout
    image.current_layout = final_layout;
}

void copy_buffer_to_image(vk::CommandBuffer cmd_buf, BufferSlice slice, RawImage& image) {
    vk::BufferImageCopy copy_region;
    copy_region.bufferOffset = slice.offset;
    // Values of 0 means tightly packed here
    copy_region.bufferRowLength = 0;
    copy_region.bufferImageHeight = 0;

    copy_region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    copy_region.imageSubresource.baseArrayLayer = 0;
    copy_region.imageSubresource.layerCount = image.layers;
    copy_region.imageSubresource.mipLevel = 0;

    copy_region.imageOffset = vk::Offset3D{ 0, 0, 0 };
    copy_region.imageExtent = vk::Extent3D{ image.size.width, image.size.height, 1 };

    cmd_buf.copyBufferToImage(slice.buffer, image.image, image.current_layout, copy_region);
}

void copy_buffer_to_image(vk::CommandBuffer cmd_buf, stl::span<BufferSlice> slices, RawImage& image) {
    // #Tag(Mipmaps)
    STL_ASSERT(slices.size() == image.layers, "Must specify as many buffer slices as there are array layers in the image");
  
    std::vector<vk::BufferImageCopy> copy_regions;
    for (auto[idx, slice] : stl::enumerate(slices.begin(), slices.end())) {
        vk::BufferImageCopy copy;
        copy.bufferOffset = slice.offset;
        copy.bufferRowLength = 0;
        copy.bufferImageHeight = 0;
        copy.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        copy.imageSubresource.baseArrayLayer = idx;
        // #Tag(Mipmaps)
        copy.imageSubresource.layerCount = 1;
        copy.imageSubresource.mipLevel = 0;

        copy.imageOffset = vk::Offset3D{ 0, 0, 0 };
        copy.imageExtent = vk::Extent3D{ image.size.width, image.size.height, 1 };

        copy_regions.push_back(copy);
    }

    // Note that all buffers have to be the same
    vk::Buffer src_buffer = slices.begin()->buffer;
    cmd_buf.copyBufferToImage(src_buffer, image.image, image.current_layout, copy_regions.size(), copy_regions.data());
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