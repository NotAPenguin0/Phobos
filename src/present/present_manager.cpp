#include <phobos/present/present_manager.hpp>

#include <phobos/util/buffer_util.hpp>
#include <phobos/renderer/render_graph.hpp>
#include <phobos/renderer/meta.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/util/memory_util.hpp>
#include <phobos/util/cache_cleanup.hpp>

#include <limits>

namespace ph {

PresentManager::PresentManager(VulkanContext& ctx, size_t max_frames_in_flight) 
    : context(ctx), max_frames_in_flight(max_frames_in_flight) {

    context.event_dispatcher.add_listener(static_cast<EventListener<WindowResizeEvent>*>(this));

    size_t const swapchain_image_count = ctx.swapchain.images.size();

    command_buffers = ctx.graphics->create_command_buffers(swapchain_image_count);

    vk::SamplerCreateInfo sampler_info;
    sampler_info.magFilter = vk::Filter::eLinear;
    sampler_info.minFilter = vk::Filter::eLinear;
    sampler_info.addressModeU = vk::SamplerAddressMode::eRepeat;
    sampler_info.addressModeV = vk::SamplerAddressMode::eRepeat;
    sampler_info.addressModeW = vk::SamplerAddressMode::eRepeat;
    sampler_info.anisotropyEnable = true;
    sampler_info.maxAnisotropy = 16;
    sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;
    sampler_info.unnormalizedCoordinates = false;
    sampler_info.compareEnable = false;
    sampler_info.compareOp = vk::CompareOp::eAlways;
    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_info.minLod = 0.0;
    sampler_info.maxLod = 64.0;
    sampler_info.mipLodBias = 0.0;
    default_sampler = ctx.device.createSampler(sampler_info);

    image_in_flight_fences = std::vector<vk::Fence>(swapchain_image_count, nullptr);
    // Initialize frame info data
    for (size_t i = 0; i < max_frames_in_flight; ++i) {
        ph::FrameInfo frame_info;
        // #Tag(Sampler)
        frame_info.default_sampler = default_sampler;
        frame_info.present_manager = this;

        vk::FenceCreateInfo fence_info;
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
        frame_info.fence = ctx.device.createFence(fence_info);

        vk::SemaphoreCreateInfo semaphore_info;
        frame_info.image_available = ctx.device.createSemaphore(semaphore_info);
        frame_info.render_finished = ctx.device.createSemaphore(semaphore_info);

        vk::DeviceSize const ubo_alignment = context.physical_device.properties.limits.minUniformBufferOffsetAlignment;
        frame_info.ubo_allocator = BufferAllocator(&context, 128 * 1024, ubo_alignment, BufferType::MappedUniformBuffer);

        vk::DeviceSize const ssbo_alignment = context.physical_device.properties.limits.minStorageBufferOffsetAlignment;
        frame_info.ssbo_allocator = BufferAllocator(&context, 512 * 1024, ssbo_alignment, BufferType::StorageBufferDynamic);

        frame_info.vbo_allocator = BufferAllocator(&context, 64 * 1024, 16, BufferType::VertexBufferDynamic);
        frame_info.ibo_allocator = BufferAllocator(&context, 64 * 1024, 16, BufferType::IndexBufferDynamic);
        frames.emplace_back(std::move(frame_info));
    }
}

FrameInfo& PresentManager::get_frame_info() {
    FrameInfo& frame = frames[frame_index];
    frame.command_buffer = command_buffers[image_index];
    frame.frame_index = frame_index;
    frame.image_index = image_index;
    frame.draw_calls = 0;
    frame.ubo_allocator.reset();
    frame.ssbo_allocator.reset();
    frame.vbo_allocator.reset();
    frame.ibo_allocator.reset();
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
    context.graphics->submit(submit_info, frame.fence);

    // Present
    vk::PresentInfoKHR present_info;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &frame.render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &context.swapchain.handle;
    present_info.pImageIndices = &image_index;
    context.graphics->present(present_info);

    // Advance to next frame
    frame_index = (frame_index + 1) % max_frames_in_flight;

    for (auto& frame : frames) {
        update_cache_resource_usage(&context, frame.descriptor_cache, max_frames_in_flight);
    }
    PerThreadContext& main_thread_ctx = context.thread_contexts[0];
    update_cache_resource_usage(&context, main_thread_ctx.framebuffer_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, main_thread_ctx.renderpass_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, main_thread_ctx.set_layout_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, main_thread_ctx.pipeline_layout_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, main_thread_ctx.pipeline_cache, max_frames_in_flight);
    update_cache_resource_usage(&context, main_thread_ctx.compute_pipeline_cache, max_frames_in_flight);
    // No update_cache_resource_usage for shader module create info as we need this information every frame.
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

    if (image_index_result.result == vk::Result::eErrorDeviceLost) {
        context.logger->write_fmt(ph::log::Severity::Fatal, "Device Lost!");
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
        context.device.destroyFence(frame.fence);
        context.device.destroySemaphore(frame.image_available);
        context.device.destroySemaphore(frame.render_finished);
        frame.descriptor_cache.get_all().clear();
        frame.ubo_allocator.destroy();
        frame.ssbo_allocator.destroy();
        frame.vbo_allocator.destroy();
        frame.ibo_allocator.destroy();
    }
    for (auto&[name, attachment] : attachments) {
        attachment.destroy();
    }
    attachments.clear();
}

RenderAttachment& PresentManager::add_color_attachment(std::string const& name) {
    return add_color_attachment(name, context.swapchain.extent);
}


RenderAttachment& PresentManager::add_color_attachment(std::string const& name, vk::Extent2D size) {
    return add_color_attachment(name, size, context.swapchain.format.format);
}

RenderAttachment& PresentManager::add_color_attachment(std::string const& name, vk::Extent2D size, vk::Format format, vk::SampleCountFlagBits samples) {
    RawImage image = create_image(context, size.width, size.height, ImageType::ColorAttachment, format, 1, 1, samples);
    ImageView view = create_image_view(context.device, image);
    return attachments[name] = RenderAttachment(&context, image, view, vk::ImageAspectFlagBits::eColor);
}

RenderAttachment& PresentManager::add_depth_attachment(std::string const& name) {
    return add_depth_attachment(name, context.swapchain.extent);
}

RenderAttachment& PresentManager::add_depth_attachment(std::string const& name, vk::Extent2D size, vk::SampleCountFlagBits samples) {
    RawImage image = create_image(context, size.width, size.height, ImageType::DepthStencilAttachment,
        vk::Format::eD32Sfloat, 1, 1, samples);
    ImageView view = create_image_view(context.device, image, vk::ImageAspectFlagBits::eDepth);

    return attachments[name] = RenderAttachment(&context, image, view, vk::ImageAspectFlagBits::eDepth);
}

RenderAttachment& PresentManager::get_attachment(std::string const& name) {
    return attachments.at(name);
}

RenderAttachment PresentManager::get_swapchain_attachment(FrameInfo& frame) {
    RawImage img;
    img.image = context.swapchain.images[frame.image_index];
    img.memory = nullptr;
    img.size = context.swapchain.extent;
    img.format = context.swapchain.format.format;
    img.layers = 1;
    return RenderAttachment::from_ref(&context, img, context.swapchain.image_views[frame.image_index]);
}

void PresentManager::on_event(WindowResizeEvent const& evt) {
    // Only handle the event if it matches our context
    if (evt.window_ctx != context.window_ctx) return;

    // Wait until all rendering is done
    context.device.waitIdle();

    for (auto view : context.swapchain.image_views) {
        destroy_image_view(context, view);
    }

    context.swapchain.image_views.clear();

    // Recreate the swapchain
    auto new_swapchain = create_swapchain(context.device, *context.window_ctx, context.physical_device, context.swapchain.handle);
    context.device.destroySwapchainKHR(context.swapchain.handle);
    context.swapchain = std::move(new_swapchain);
    
    context.event_dispatcher.fire_event(SwapchainRecreateEvent{evt.window_ctx});
}


}