#include <phobos/renderer/renderer.hpp>

namespace ph {

Renderer::Renderer(VulkanContext& context) : ctx(context) {

}

void Renderer::render_frame(FrameInfo& info, RenderGraph const& graph) {
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

    render_pass_info.clearValueCount = 1;
    vk::ClearValue clear_value = graph.clear_color;
    render_pass_info.pClearValues = &clear_value;

    cmd_buffer.beginRenderPass(render_pass_info, vk::SubpassContents::eInline);
    
    vk::Pipeline pipeline = ctx.pipelines.get_pipeline(PipelineID::eGeneric);
    cmd_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    for (auto& draw : graph.draw_commands) {
        Mesh* mesh = graph.meshes[draw.mesh_index];

        vk::DeviceSize const offset = 0;
        cmd_buffer.bindVertexBuffers(0, mesh->get_vertices(), offset);
        cmd_buffer.bindIndexBuffer(mesh->get_indices(), 0, vk::IndexType::eUint32);
        cmd_buffer.drawIndexed(mesh->get_index_count(), 1, 0, 0, 0);
        info.draw_calls++;
    }
    
    cmd_buffer.endRenderPass();
    cmd_buffer.end();

}

}