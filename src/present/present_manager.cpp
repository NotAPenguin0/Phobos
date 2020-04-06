#include <phobos/present/present_manager.hpp>

#include <phobos/util/buffer_util.hpp>
#include <phobos/renderer/dynamic_gpu_buffer.hpp>
#include <phobos/renderer/render_graph.hpp>
#include <phobos/renderer/meta.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/util/cmdbuf_util.hpp>
#include <phobos/util/memory_util.hpp>
#include <phobos/util/cache_cleanup.hpp>

#undef min
#undef max
#include <limits>

namespace ph {


static MappedUBO create_mapped_ubo(VulkanContext& ctx, size_t const size) {
    MappedUBO ubo;
    create_buffer(ctx, size, vk::BufferUsageFlagBits::eUniformBuffer, 
        vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, ubo.buffer, ubo.memory);
    ubo.size = size;
    ubo.ptr = ctx.device.mapMemory(ubo.memory, 0, VK_WHOLE_SIZE);
    return ubo;
}

PresentManager::PresentManager(VulkanContext& ctx, size_t max_frames_in_flight) 
    : context(ctx), max_frames_in_flight(max_frames_in_flight) {

    context.event_dispatcher.add_listener(static_cast<EventListener<WindowResizeEvent>*>(this));

    vk::CommandPoolCreateInfo command_pool_info;
    command_pool_info.queueFamilyIndex = ctx.physical_device.queue_families.graphics_family.value();
    // We're going to reset the command buffers each frame and re-record them
    command_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    command_pool = ctx.device.createCommandPool(command_pool_info);

    size_t const swapchain_image_count = ctx.swapchain.images.size();

    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandBufferCount = swapchain_image_count;
    alloc_info.commandPool = command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffers = ctx.device.allocateCommandBuffers(alloc_info);

    vk::SamplerCreateInfo sampler_info;
    sampler_info.magFilter = vk::Filter::eLinear;
    sampler_info.minFilter = vk::Filter::eLinear;
    sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
    sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
    sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
    sampler_info.anisotropyEnable = true;
    sampler_info.maxAnisotropy = 8;
    sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
    sampler_info.unnormalizedCoordinates = false;
    sampler_info.compareEnable = false;
    sampler_info.compareOp = vk::CompareOp::eAlways;
    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_info.minLod = 0.0;
    sampler_info.maxLod = 0.0;
    sampler_info.mipLodBias = 0.0;
    default_sampler = ctx.device.createSampler(sampler_info);

    // TODO: Allow for more descriptors
    vk::DescriptorPoolSize sizes[] = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1000),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 1000)
    };
     
    vk::DescriptorPoolCreateInfo fixed_descriptor_pool_info;
    fixed_descriptor_pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    fixed_descriptor_pool_info.poolSizeCount = sizeof(sizes) / sizeof(vk::DescriptorPoolSize);
    fixed_descriptor_pool_info.pPoolSizes = sizes;
    fixed_descriptor_pool_info.maxSets = sizeof(sizes) / sizeof(vk::DescriptorPoolSize) * 1000;
    main_descriptor_pool = ctx.device.createDescriptorPool(fixed_descriptor_pool_info);

    image_in_flight_fences = stl::vector<vk::Fence>(swapchain_image_count, nullptr);
    frames.resize(max_frames_in_flight);
    // Initialize frame info data
    for (auto& frame_info : frames) {
        // #Tag(Sampler)
        frame_info.default_sampler = default_sampler;
        frame_info.present_manager = this;
        frame_info.descriptor_pool = main_descriptor_pool;

        vk::FenceCreateInfo fence_info;
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
        frame_info.fence = ctx.device.createFence(fence_info);

        vk::SemaphoreCreateInfo semaphore_info;
        frame_info.image_available = ctx.device.createSemaphore(semaphore_info);
        frame_info.render_finished = ctx.device.createSemaphore(semaphore_info);

        // Create camera UBO for this frame
        // Note that the camera position is a vec3, but it is aligned to a vec4
        frame_info.vp_ubo = create_mapped_ubo(ctx, sizeof(glm::mat4) + sizeof(glm::vec4));
        frame_info.lights = create_mapped_ubo(ctx, sizeof(PointLight) * meta::max_lights_per_type + sizeof(uint32_t));

        // Create transform data SSBO for this frame
        frame_info.transform_ssbo = DynamicGpuBuffer(ctx);
        // Start with 32 transforms (arbitrary number)
        static constexpr stl::size_t transforms_begin_size = 32;
        frame_info.transform_ssbo.create(transforms_begin_size * sizeof(glm::mat4));
    }
}

FrameInfo& PresentManager::get_frame_info() {
    FrameInfo& frame = frames[frame_index];
    frame.command_buffer = command_buffers[image_index];
    frame.frame_index = frame_index;
    frame.image_index = image_index;
    frame.draw_calls = 0;
    return frame;
}

void PresentManager::present_frame(FrameInfo& frame) {
    // submit command buffer
    vk::SubmitInfo submit_info;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &frame.image_available;
    vk::PipelineStageFlags wait_stages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &frame.command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &frame.render_finished;
    context.device.resetFences(frame.fence);
    context.graphics_queue.submit(submit_info, frame.fence);

    // Present
    vk::PresentInfoKHR present_info;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &frame.render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &context.swapchain.handle;
    present_info.pImageIndices = &image_index;
    context.graphics_queue.presentKHR(present_info);
    // Advance to next frame
    frame_index = (frame_index + 1) % max_frames_in_flight;

    for (auto& frame : frames) {
        update_cache_resource_usage(&context, frame.descriptor_cache, max_frames_in_flight);
    }
    update_cache_resource_usage(&context, context.framebuffer_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, context.renderpass_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, context.set_layout_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, context.pipeline_layout_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, context.pipeline_cache, max_frames_in_flight);
}

void PresentManager::wait_for_available_frame() {
    context.device.waitForFences(frames[frame_index].fence, true, std::numeric_limits<uint32_t>::max());
    frames[frame_index].render_graph = nullptr;

    // Get image
    vk::ResultValue<uint32_t> image_index_result = context.device.acquireNextImageKHR(context.swapchain.handle, 
                                    std::numeric_limits<uint32_t>::max(), frames[frame_index].image_available, nullptr);
    if (image_index_result.result == vk::Result::eSuboptimalKHR || 
        image_index_result.result == vk::Result::eErrorOutOfDateKHR) {
        context.event_dispatcher.fire_event(
            WindowResizeEvent{context.window_ctx, (int32_t)context.window_ctx->width, (int32_t)context.window_ctx->height});
    }
    image_index = image_index_result.value;
    // Make sure its available
    if (image_in_flight_fences[image_index]) {
        context.device.waitForFences(image_in_flight_fences[image_index], true, std::numeric_limits<uint32_t>::max());
    }

    // Mark the image as in use
    image_in_flight_fences[image_index] = frames[frame_index].fence;
}


void PresentManager::destroy() {
    context.device.destroySampler(default_sampler);
    for (auto& frame : frames) {
        context.device.destroyBuffer(frame.lights.buffer);
        context.device.unmapMemory(frame.lights.memory);
        context.device.freeMemory(frame.lights.memory);
        frame.transform_ssbo.destroy();
        context.device.destroyFence(frame.fence);
        context.device.destroySemaphore(frame.image_available);
        context.device.destroySemaphore(frame.render_finished);
        context.device.destroyBuffer(frame.vp_ubo.buffer);
        context.device.unmapMemory(frame.vp_ubo.memory);
        context.device.freeMemory(frame.vp_ubo.memory);
        frame.descriptor_cache.get_all().clear();
    }
    for (auto&[name, attachment] : attachments) {
        attachment.destroy();
    }
    attachments.clear();
    context.device.destroyDescriptorPool(main_descriptor_pool);
    context.device.destroyCommandPool(command_pool);
}

RenderAttachment PresentManager::add_color_attachment(std::string const& name) {
    vk::Image image;
    vk::DeviceMemory memory;
    vk::ImageUsageFlags const usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    create_image(context, context.swapchain.extent.width, context.swapchain.extent.height,
        context.swapchain.format.format, vk::ImageTiling::eOptimal, 
        usage, vk::MemoryPropertyFlagBits::eDeviceLocal,
        image, memory);
    vk::ImageView view = create_image_view(context.device, image, context.swapchain.format.format);
    return attachments[name] = std::move(RenderAttachment(&context, image, memory, view, 
                context.swapchain.extent.width, context.swapchain.extent.height,
                context.swapchain.format.format, usage,
                vk::ImageAspectFlagBits::eColor));
}

RenderAttachment PresentManager::add_depth_attachment(std::string const& name) {
    vk::Image image;
    vk::DeviceMemory memory;
    vk::ImageUsageFlags const usage = vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransferSrc |
        vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    create_image(context, context.swapchain.extent.width, context.swapchain.extent.height,
        vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal, 
        usage, vk::MemoryPropertyFlagBits::eDeviceLocal,
        image, memory);
    vk::ImageView view = create_image_view(context.device, image, vk::Format::eD32Sfloat, vk::ImageAspectFlagBits::eDepth);

    return attachments[name] = std::move(RenderAttachment(&context, image, memory, view,
        context.swapchain.extent.width, context.swapchain.extent.height, vk::Format::eD32Sfloat, usage, 
        vk::ImageAspectFlagBits::eDepth));
}

RenderAttachment& PresentManager::get_attachment(std::string const& name) {
    return attachments.at(name);
}

RenderAttachment PresentManager::get_swapchain_attachment(FrameInfo& frame) {
    return RenderAttachment::from_ref(&context, context.swapchain.images[frame.image_index],
        nullptr, context.swapchain.image_views[frame.image_index], 
        context.swapchain.extent.width, context.swapchain.extent.height, context.swapchain.format.format);
}

void PresentManager::on_event(WindowResizeEvent const& evt) {
    // Only handle the event if it matches our context
    if (evt.window_ctx != context.window_ctx) return;

    // Wait until all rendering is done
    context.device.waitIdle();

    for (auto view : context.swapchain.image_views) {
        context.device.destroyImageView(view);
    }

    context.swapchain.image_views.clear();

    // Recreate the swapchain
    auto new_swapchain = create_swapchain(context.device, *context.window_ctx, context.physical_device, context.swapchain.handle);
    context.device.destroySwapchainKHR(context.swapchain.handle);
    context.swapchain = std::move(new_swapchain);
    
    context.event_dispatcher.fire_event(SwapchainRecreateEvent{evt.window_ctx});
}


}