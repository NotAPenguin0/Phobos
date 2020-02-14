#include <phobos/present/present_manager.hpp>

#undef min
#undef max
#include <limits>

namespace ph {

PresentManager::PresentManager(VulkanContext& ctx, size_t max_frames_in_flight) 
    : context(ctx), max_frames_in_flight(max_frames_in_flight) {
    vk::CommandPoolCreateInfo command_pool_info;
    command_pool_info.queueFamilyIndex = ctx.physical_device.queue_families.graphics_family.value();
    // We're going to reset the command buffers each frame and re-record them
    command_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    command_pool = ctx.device.createCommandPool(command_pool_info);

    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandBufferCount = ctx.swapchain.images.size();
    alloc_info.commandPool = command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffers = ctx.device.allocateCommandBuffers(alloc_info);

    image_in_flight_fences = std::vector<vk::Fence>(ctx.swapchain.images.size(), nullptr);
    frames.resize(max_frames_in_flight);
    // Initialize frame info data
    for (auto& frame_info : frames) {
        vk::FenceCreateInfo fence_info;
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
        frame_info.fence = ctx.device.createFence(fence_info);

        vk::SemaphoreCreateInfo semaphore_info;
        frame_info.image_available = ctx.device.createSemaphore(semaphore_info);
        frame_info.render_finished = ctx.device.createSemaphore(semaphore_info);
    }
}

FrameInfo& PresentManager::get_frame_info() {
    FrameInfo& frame = frames[frame_index];
    frame.framebuffer = context.swapchain.framebuffers[image_index];
    frame.command_buffer = command_buffers[image_index];
    frame.frame_index = frame_index;
    frame.image_index = image_index;
    frame.image = context.swapchain.images[image_index];
    frame.extra_command_buffers.clear();
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

    std::vector<vk::CommandBuffer>& cmd_buffers = frame.extra_command_buffers;
    cmd_buffers.insert(cmd_buffers.begin(), frame.command_buffer);

    submit_info.commandBufferCount = cmd_buffers.size();
    submit_info.pCommandBuffers = cmd_buffers.data();

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
}

void PresentManager::wait_for_available_frame() {
    context.device.waitForFences(frames[frame_index].fence, true, std::numeric_limits<uint32_t>::max());
    // Get image
    image_index = context.device.acquireNextImageKHR(context.swapchain.handle, 
                                    std::numeric_limits<uint32_t>::max(), frames[frame_index].image_available, nullptr).value;
    // Make sure its available
    if (image_in_flight_fences[image_index]) {
        context.device.waitForFences(image_in_flight_fences[image_index], true, std::numeric_limits<uint32_t>::max());
    }

    // Mark the image as in use
    image_in_flight_fences[image_index] = frames[frame_index].fence;
}


void PresentManager::destroy() {
    for (auto& frame : frames) {
        context.device.destroyFence(frame.fence);
        context.device.destroySemaphore(frame.image_available);
        context.device.destroySemaphore(frame.render_finished);
    }
    context.device.destroyCommandPool(command_pool);
}


}