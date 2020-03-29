#include <phobos/present/present_manager.hpp>

#include <phobos/util/buffer_util.hpp>
#include <phobos/renderer/instancing_buffer.hpp>
#include <phobos/renderer/render_graph.hpp>
#include <phobos/renderer/meta.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/util/cmdbuf_util.hpp>
#include <phobos/util/memory_util.hpp>

#undef min
#undef max
#include <limits>

namespace ph {

static void create_lights_ubo(VulkanContext& ctx, MappedUBO& ubo) {
    size_t const size = meta::max_lights_per_type * sizeof(PointLight) + sizeof(uint32_t);
    create_buffer(ctx, size, vk::BufferUsageFlagBits::eUniformBuffer, 
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
    ubo.buffer, ubo.memory);
    ubo.size = size;
    ubo.ptr = ctx.device.mapMemory(ubo.memory, 0, VK_WHOLE_SIZE);
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



    vk::DescriptorPoolSize sizes[] = {
        vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2 * swapchain_image_count),
        vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, swapchain_image_count),
        vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, meta::max_textures_bound * swapchain_image_count)
    };
     
    vk::DescriptorPoolCreateInfo fixed_descriptor_pool_info;
    fixed_descriptor_pool_info.poolSizeCount = sizeof(sizes) / sizeof(sizes[0]);
    fixed_descriptor_pool_info.pPoolSizes = sizes;
    // Add some extra space
    fixed_descriptor_pool_info.maxSets = swapchain_image_count * 3;

    fixed_descriptor_pool = ctx.device.createDescriptorPool(fixed_descriptor_pool_info);

    image_in_flight_fences = stl::vector<vk::Fence>(swapchain_image_count, nullptr);
    frames.resize(max_frames_in_flight);
    // Initialize frame info data
    for (auto& frame_info : frames) {
        frame_info.default_sampler = default_sampler;
        frame_info.present_manager = this;

        vk::FenceCreateInfo fence_info;
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;
        frame_info.fence = ctx.device.createFence(fence_info);

        vk::SemaphoreCreateInfo semaphore_info;
        frame_info.image_available = ctx.device.createSemaphore(semaphore_info);
        frame_info.render_finished = ctx.device.createSemaphore(semaphore_info);

        frame_info.swapchain_target = RenderTarget(&context);

        // Create camera UBO for this frame
        // Note that the camera position is a vec3, but it is aligned to a vec4
        size_t const camera_buffer_size = sizeof(glm::mat4) + sizeof(glm::vec4);
        create_buffer(ctx, camera_buffer_size, vk::BufferUsageFlagBits::eUniformBuffer, 
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, 
            frame_info.vp_ubo.buffer, frame_info.vp_ubo.memory);
        frame_info.vp_ubo.size = camera_buffer_size;
        frame_info.vp_ubo.ptr = ctx.device.mapMemory(frame_info.vp_ubo.memory, 0, VK_WHOLE_SIZE);

        create_lights_ubo(ctx, frame_info.lights);

        // Create instance data SSBO for this frame
        frame_info.instance_ssbo = InstancingBuffer(ctx);
        // Start with 32 instances (arbitrary number)
        static constexpr stl::size_t transforms_begin_size = 32;
        frame_info.instance_ssbo.create(transforms_begin_size * sizeof(glm::mat4));

        // Create descriptor sets for this frame
        PipelineLayout vp_layout = ctx.pipeline_layouts.get_layout(PipelineLayoutID::eDefault);
        vk::DescriptorSetAllocateInfo descriptor_set_info;
        descriptor_set_info.descriptorPool = fixed_descriptor_pool;
        descriptor_set_info.descriptorSetCount = 1;
        descriptor_set_info.pSetLayouts = &vp_layout.descriptor_set_layout;

        vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
        uint32_t counts[1] { meta::max_textures_bound };
        variable_count_info.descriptorSetCount = 1;
        variable_count_info.pDescriptorCounts = counts;

        descriptor_set_info.pNext = &variable_count_info;

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

        vk::DescriptorBufferInfo lights_buffer_write;
        lights_buffer_write.buffer = frame_info.lights.buffer;
        lights_buffer_write.offset = 0;
        lights_buffer_write.range = frame_info.lights.size;
 
        constexpr size_t write_cnt = 3;
        constexpr size_t vp_write = 0;
        constexpr size_t instance_write = 1;
        constexpr size_t lights_write = 2;
        vk::WriteDescriptorSet writes[write_cnt];
        writes[vp_write].descriptorCount = 1;
        writes[vp_write].descriptorType = vk::DescriptorType::eUniformBuffer;
        writes[vp_write].dstSet = frame_info.fixed_descriptor_set;
        writes[vp_write].dstBinding = meta::bindings::generic::camera;
        writes[vp_write].dstArrayElement = 0;
        writes[vp_write].pBufferInfo = &vp_buffer_write;

        writes[instance_write].descriptorCount = 1;
        writes[instance_write].descriptorType = vk::DescriptorType::eStorageBuffer;
        writes[instance_write].dstSet = frame_info.fixed_descriptor_set;
        writes[instance_write].dstBinding = meta::bindings::generic::instance_data;
        writes[instance_write].dstArrayElement = 0;
        writes[instance_write].pBufferInfo = &instance_buffer_write;

        writes[lights_write].descriptorCount = 1;
        writes[lights_write].descriptorType = vk::DescriptorType::eUniformBuffer;
        writes[lights_write].dstSet = frame_info.fixed_descriptor_set;
        writes[lights_write].dstBinding = meta::bindings::generic::lights;
        writes[lights_write].dstArrayElement = 0;
        writes[lights_write].pBufferInfo = &lights_buffer_write;

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
    // Make sure frame_index and image_index are set here!
    frame.swapchain_color = get_swapchain_attachment(frame);

    stl::vector<RenderAttachment> swapchain_attachments;
    swapchain_attachments.push_back(frame.swapchain_color);
    frame.swapchain_target = 
        std::move(RenderTarget(&context, context.swapchain_render_pass, swapchain_attachments));
    // Clear previous draw data
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

    // When this frame is done we can deallocate the RenderGraph owned by it.
    // Note that we might have to change this when caching vkRenderPass and vkFramebuffer creation
    delete frames[frame_index].render_graph;
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
        frame.instance_ssbo.destroy();
        context.device.destroyFence(frame.fence);
        context.device.destroySemaphore(frame.image_available);
        context.device.destroySemaphore(frame.render_finished);
        context.device.destroyBuffer(frame.vp_ubo.buffer);
        context.device.unmapMemory(frame.vp_ubo.memory);
        context.device.freeMemory(frame.vp_ubo.memory);
        frame.swapchain_target.destroy();
        frame.offscreen_target.destroy();
        frame.swapchain_color.destroy();
        delete frame.render_graph;
    }
    for (auto&[name, attachment] : attachments) {
        attachment.destroy();
    }
    attachments.clear();
    context.device.destroyDescriptorPool(fixed_descriptor_pool);
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

    for (auto framebuf : context.swapchain.framebuffers) {
        context.device.destroyFramebuffer(framebuf);
    }

    context.swapchain.framebuffers.clear();

    for (auto view : context.swapchain.image_views) {
        context.device.destroyImageView(view);
    }

    context.swapchain.image_views.clear();

    // Recreate the swapchain
    auto new_swapchain = create_swapchain(context.device, *context.window_ctx, context.physical_device, context.swapchain.handle);
    context.device.destroySwapchainKHR(context.swapchain.handle);
    context.swapchain = std::move(new_swapchain);

    // Recreate renderpass
    context.event_dispatcher.fire_event(SwapchainRecreateEvent{evt.window_ctx});

    // Recreate framebuffers. This has to be done after the renderpass has been recreated
    create_swapchain_framebuffers(context, context.swapchain);
}


}