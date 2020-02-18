#include <phobos/present/present_manager.hpp>

#include <phobos/util/buffer_util.hpp>
#include <phobos/renderer/instancing_buffer.hpp>
#include <phobos/renderer/render_graph.hpp>

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

    size_t const swapchain_image_count = ctx.swapchain.images.size();

    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandBufferCount = swapchain_image_count;
    alloc_info.commandPool = command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    command_buffers = ctx.device.allocateCommandBuffers(alloc_info);

    vk::DescriptorPoolSize sizes[] = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, swapchain_image_count),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, swapchain_image_count)
    };

    vk::DescriptorPoolCreateInfo fixed_descriptor_pool_info;
    fixed_descriptor_pool_info.poolSizeCount = sizeof(sizes) / sizeof(sizes[0]);
    fixed_descriptor_pool_info.pPoolSizes = sizes;
    fixed_descriptor_pool_info.maxSets = swapchain_image_count;

    fixed_descriptor_pool = ctx.device.createDescriptorPool(fixed_descriptor_pool_info);

    image_in_flight_fences = std::vector<vk::Fence>(swapchain_image_count, nullptr);
    frames.resize(max_frames_in_flight);
    // Initialize frame info data
    for (auto& frame_info : frames) {
        vk::FenceCreateInfo fence_info;
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
        frame_info.fence = ctx.device.createFence(fence_info);

        vk::SemaphoreCreateInfo semaphore_info;
        frame_info.image_available = ctx.device.createSemaphore(semaphore_info);
        frame_info.render_finished = ctx.device.createSemaphore(semaphore_info);

        // Create VP UBO for this frame
        create_buffer(ctx, 2 * 16 * sizeof(float), vk::BufferUsageFlagBits::eUniformBuffer, 
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
            frame_info.vp_ubo.buffer, frame_info.vp_ubo.memory);
        ctx.device.bindBufferMemory(frame_info.vp_ubo.buffer, frame_info.vp_ubo.memory, 0);
        frame_info.vp_ubo.size = 2 * 16 * sizeof(float);
        frame_info.vp_ubo.ptr = ctx.device.mapMemory(frame_info.vp_ubo.memory, 0, 2 * 16 * sizeof(float));

        // Create instance data UBO for this frame
        frame_info.instance_ssbo = InstancingBuffer(ctx);
        // Start with 32 instances (arbitrary number)
        frame_info.instance_ssbo.create(32 * sizeof(RenderGraph::Instance));

        // Create descriptor sets for this frame
        PipelineLayout vp_layout = ctx.pipeline_layouts.get_layout(PipelineLayoutID::eMVPOnly);
        vk::DescriptorSetAllocateInfo descriptor_set_info;
        descriptor_set_info.descriptorPool = fixed_descriptor_pool;
        descriptor_set_info.descriptorSetCount = 1;
        descriptor_set_info.pSetLayouts = &vp_layout.descriptor_set_layout;
        frame_info.fixed_descriptor_set = ctx.device.allocateDescriptorSets(descriptor_set_info)[0];

        // Setup descriptor set to point to right VP UBO
        vk::DescriptorBufferInfo vp_buffer_write;
        vp_buffer_write.buffer = frame_info.vp_ubo.buffer;
        vp_buffer_write.offset = 0;
        vp_buffer_write.range = frame_info.vp_ubo.size;
        
        vk::DescriptorBufferInfo instance_buffer_write;
        instance_buffer_write.buffer = frame_info.instance_ssbo.buffer_handle();
        instance_buffer_write.offset = 0;
        instance_buffer_write.range = frame_info.instance_ssbo.size();

        constexpr size_t write_cnt = 2;
        constexpr size_t vp_write = 0;
        constexpr size_t instance_write = 1;
        vk::WriteDescriptorSet writes[write_cnt];
        writes[vp_write].descriptorCount = 1;
        writes[vp_write].descriptorType = vk::DescriptorType::eUniformBuffer;
        writes[vp_write].dstSet = frame_info.fixed_descriptor_set;
        writes[vp_write].dstBinding = 0;
        writes[vp_write].dstArrayElement = 0;
        writes[vp_write].pBufferInfo = &vp_buffer_write;

        writes[instance_write].descriptorCount = 1;
        writes[instance_write].descriptorType = vk::DescriptorType::eStorageBuffer;
        writes[instance_write].dstSet = frame_info.fixed_descriptor_set;
        writes[instance_write].dstBinding = 1;
        writes[instance_write].dstArrayElement = 0;
        writes[instance_write].pBufferInfo = &instance_buffer_write;

        ctx.device.updateDescriptorSets(write_cnt, writes, 0, nullptr);
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
        frame.instance_ssbo.destroy();
        context.device.destroyFence(frame.fence);
        context.device.destroySemaphore(frame.image_available);
        context.device.destroySemaphore(frame.render_finished);
        context.device.destroyBuffer(frame.vp_ubo.buffer);
        context.device.unmapMemory(frame.vp_ubo.memory);
        context.device.freeMemory(frame.vp_ubo.memory);
    }
    context.device.destroyDescriptorPool(fixed_descriptor_pool);
    context.device.destroyCommandPool(command_pool);
}


}