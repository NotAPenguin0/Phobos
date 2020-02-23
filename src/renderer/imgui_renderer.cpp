#include <phobos/renderer/imgui_renderer.hpp>

#include <imgui/imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_mimas.h>

namespace ph {

static vk::DescriptorPool create_imgui_descriptor_pool(ph::VulkanContext& ctx) {
    vk::DescriptorPoolSize pool_sizes[] =
    {
        { vk::DescriptorType::eSampler, 1000 },
        { vk::DescriptorType::eCombinedImageSampler, 1000 },
        { vk::DescriptorType::eSampledImage, 1000 },
        { vk::DescriptorType::eStorageImage, 1000 },
        { vk::DescriptorType::eUniformTexelBuffer, 1000 },
        { vk::DescriptorType::eStorageTexelBuffer, 1000 },
        { vk::DescriptorType::eUniformBuffer, 1000 },
        { vk::DescriptorType::eStorageBuffer, 1000 },
        { vk::DescriptorType::eUniformBufferDynamic, 1000 },
        { vk::DescriptorType::eStorageBufferDynamic, 1000 },
        { vk::DescriptorType::eInputAttachment, 1000 }
    };
    vk::DescriptorPoolCreateInfo pool_info = {};
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    return ctx.device.createDescriptorPool(pool_info);
}

static void load_imgui_fonts(ph::VulkanContext& ctx, vk::CommandPool command_pool) {
    vk::CommandBufferAllocateInfo buf_info;
    buf_info.commandBufferCount = 1;
    buf_info.commandPool = command_pool;
    buf_info.level = vk::CommandBufferLevel::ePrimary;
    vk::CommandBuffer command_buffer = ctx.device.allocateCommandBuffers(buf_info)[0];

    vk::CommandBufferBeginInfo begin_info = {};
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    command_buffer.begin(begin_info);
    
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

    vk::SubmitInfo end_info = {};
    end_info.commandBufferCount = 1;
    end_info.pCommandBuffers = &command_buffer;
    command_buffer.end();

    ctx.graphics_queue.submit(end_info, nullptr);

    ctx.device.waitIdle();
    ImGui_ImplVulkan_DestroyFontUploadObjects();
    ctx.device.freeCommandBuffers(command_pool, command_buffer);
}

// an imgui renderpass doesn't clear on load
static vk::RenderPass create_imgui_renderpass(VulkanContext& ctx) {
    // Create attachment
    vk::AttachmentDescription color_attachment;
    color_attachment.format = ctx.swapchain.format.format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eLoad;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eColorAttachmentOptimal;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference color_attachment_ref;
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    // Create subpass
    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    // Setup subpass dependencies
    vk::SubpassDependency dependency;
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo info;
    info.attachmentCount = 1;
    info.pAttachments = &color_attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    info.dependencyCount = 1;
    info.pDependencies = &dependency;

    return ctx.device.createRenderPass(info);
}

ImGuiRenderer::ImGuiRenderer(WindowContext& window_ctx, VulkanContext& context) : ctx(context) {
    auto& io = ImGui::GetIO();
    
    descriptor_pool = create_imgui_descriptor_pool(context);

    // Create command pool
    vk::CommandPoolCreateInfo command_pool_info;
    command_pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
    command_pool = context.device.createCommandPool(command_pool_info);

    // Initialize imgui for Vulkan. Make sure to create the renderpass first as we'll need it
    render_pass = create_imgui_renderpass(context);
 
    ImGui_ImplMimas_InitForVulkan(window_ctx.handle);
    ImGui_ImplVulkan_InitInfo info = {};
    info.Instance = context.instance;
    info.PhysicalDevice = context.physical_device.handle;
    info.Device = context.device;
    info.QueueFamily = context.physical_device.queue_families.graphics_family.value();
    info.Queue = context.graphics_queue;
    info.DescriptorPool = descriptor_pool;
    info.Allocator = nullptr;
    info.MinImageCount = 2;
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.PipelineCache = VK_NULL_HANDLE;
    info.ImageCount = context.swapchain.images.size();
    info.CheckVkResultFn = nullptr;
    ImGui_ImplVulkan_Init(&info, render_pass);

    // Initialize fonts
    io.Fonts->AddFontDefault();
    load_imgui_fonts(context, command_pool);

    // Create command buffers
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = context.swapchain.images.size();
    alloc_info.commandPool = command_pool;
    cmd_buffers = context.device.allocateCommandBuffers(alloc_info);

    // Create framebuffers
    framebuffers.resize(context.swapchain.images.size());
    for (size_t i = 0; i < context.swapchain.images.size(); ++i) {
        vk::FramebufferCreateInfo framebuf_info;
        framebuf_info.renderPass = render_pass;
        framebuf_info.attachmentCount = 1;
        vk::ImageView attachments[1] = { context.swapchain.image_views[i]};
        framebuf_info.pAttachments = attachments;
        framebuf_info.width = context.swapchain.extent.width;
        framebuf_info.height = context.swapchain.extent.height;
        framebuf_info.layers = 1;

        framebuffers[i] = context.device.createFramebuffer(framebuf_info);
    }
}

void ImGuiRenderer::destroy() {
    ctx.device.destroyCommandPool(command_pool);
    ctx.device.destroyDescriptorPool(descriptor_pool);
    ctx.device.destroyRenderPass(render_pass);
    for (auto framebuf : framebuffers) {
        ctx.device.destroyFramebuffer(framebuf);
    }
    framebuffers.clear();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplMimas_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiRenderer::begin_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplMimas_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderer::render_frame(FrameInfo& info) {
    vk::CommandBuffer cmd_buffer = cmd_buffers[info.image_index]; 

    vk::CommandBufferBeginInfo begin_info;
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd_buffer.begin(begin_info);

    // Add a barrier to make sure the regular rendering is completed before we start imgui rendering
    vk::ImageMemoryBarrier barrier;
    barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
    barrier.image = info.image;
    barrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;

    cmd_buffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eColorAttachmentOutput, 
                               vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);

    // Start render pass
    vk::RenderPassBeginInfo render_pass_info;
    render_pass_info.renderPass = render_pass;
    render_pass_info.framebuffer = framebuffers[info.image_index];
    render_pass_info.renderArea.offset = vk::Offset2D{0, 0};
    render_pass_info.renderArea.extent = ctx.swapchain.extent;

    cmd_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);

    // Render imgui
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buffer);
    
    cmd_buffer.endRenderPass();
    cmd_buffer.end();

    // Add the command buffer to the list of command buffers to submit
    info.extra_command_buffers.push_back(cmd_buffer);
}

void ImGuiRenderer::on_event(SwapchainRecreateEvent const& evt) {
    // Destroy old resources
    for (auto framebuf : framebuffers) {
        ctx.device.destroyFramebuffer(framebuf);
    }
    framebuffers.clear();
    ctx.device.destroyRenderPass(render_pass);

    // Recreate renderpass first       
    render_pass = create_imgui_renderpass(ctx);

    // Recreate framebuffer
    framebuffers.resize(ctx.swapchain.images.size());
    for (size_t i = 0; i < ctx.swapchain.images.size(); ++i) {
        vk::FramebufferCreateInfo framebuf_info;
        framebuf_info.renderPass = render_pass;
        framebuf_info.attachmentCount = 1;
        vk::ImageView attachments[1] = { ctx.swapchain.image_views[i]};
        framebuf_info.pAttachments = attachments;
        framebuf_info.width = ctx.swapchain.extent.width;
        framebuf_info.height = ctx.swapchain.extent.height;
        framebuf_info.layers = 1;

        framebuffers[i] = ctx.device.createFramebuffer(framebuf_info);
    }
}

}