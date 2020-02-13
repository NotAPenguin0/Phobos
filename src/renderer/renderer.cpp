#include <phobos/renderer/renderer.hpp>

namespace ph {

Renderer::Renderer(VulkanContext& context) : ctx(context) {

}

void Renderer::render_frame(FrameInfo& info) {
    vk::CommandBuffer cmd_buffer = info.command_buffer;
    // Record command buffer
    vk::CommandBufferBeginInfo begin_info;
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd_buffer.begin(begin_info);

    // Start render pass
    vk::RenderPassBeginInfo render_pass_info;
    render_pass_info.renderPass = ctx.default_render_pass;
    render_pass_info.framebuffer = info.framebuffer;
    render_pass_info.renderArea.offset = vk::Offset2D{0, 0};
    render_pass_info.renderArea.extent = ctx.swapchain.extent;

    vk::ClearValue clear_color = vk::ClearColorValue(std::array<float, 4>{{1.0f, 0.0f, 0.0f, 1.0f}});
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    cmd_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
    
    vk::Pipeline pipeline = ctx.pipelines.get_pipeline(PipelineID::eGeneric);
    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    // Drawcalls go here. For now, we have none, since there is no rendergraph yet
    
    cmd_buffer.endRenderPass();
    cmd_buffer.end();

}

}