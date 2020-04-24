#include <phobos/fixed/skybox_renderer.hpp>

#include <phobos/fixed/util.hpp>

namespace ph::fixed {


static constexpr float skybox_vertices[] = { 
    -1, -1, -1, 1, 1, -1, 1, -1, -1, 1, 1, -1,
    -1, -1, -1, -1, 1, -1, -1, -1, 1, 1, -1, 1,
    1, 1, 1, 1, 1, 1, -1, 1, 1, 1, -1, 1,
    -1, 1, -1, -1, -1, -1, -1, 1, 1, -1, -1, -1,
    -1, -1, 1, -1, 1, 1, 1, 1, 1, 1, -1, -1,
    1, 1, -1, 1, -1, -1,1, 1, 1, 1, -1, 1,
    -1, -1, -1, 1, -1, -1,1, -1, 1, 1, -1, 1,
    -1, -1, 1, -1, -1, -1, -1, 1, -1, 1, 1, 1,
     1, 1, -1, 1, 1, 1, -1, 1, -1, -1, 1, 1,
};

void SkyboxRenderer::execute(ph::FrameInfo& frame, ph::CommandBuffer& cmd_buf) {
    if (!skybox) { return; }

    ph::RenderPass* pass_ptr = cmd_buf.get_active_renderpass();
    STL_ASSERT(pass_ptr, "SkyboxRenderer::execute called outside an active renderpass");
    ph::RenderPass& pass = *pass_ptr;

    BufferSlice ubo = cmd_buf.allocate_scratch_ubo(sizeof(glm::mat4));
    std::memcpy(ubo.data, &frame.render_graph->projection[0][0], 16 * sizeof(float));

    BufferSlice cube_mesh = cmd_buf.allocate_scratch_vbo(sizeof(skybox_vertices));
    std::memcpy(cube_mesh.data, skybox_vertices, sizeof(skybox_vertices));

    Pipeline pipeline = get_pipeline(ctx, "builtin_skybox", pass);
    PipelineCreateInfo const* pci = ctx->pipelines.get_named_pipeline("builtin_skybox");
    STL_ASSERT(pci, "skybox pipeline not created");
    ShaderInfo const& shader_info = pci->shader_info;

    cmd_buf.bind_pipeline(pipeline);
    DescriptorSetBinding bindings;
    bindings.add(make_descriptor(shader_info["transform"], ubo));
    // TODO: Custom sampler?
    bindings.add(make_descriptor(shader_info["skybox"], skybox->view_handle(), frame.default_sampler));
    vk::DescriptorSet set = renderer->get_descriptor(frame, pass, bindings);

    cmd_buf.bind_descriptor_set(0, set);
    cmd_buf.bind_vertex_buffer(0, cube_mesh);
    cmd_buf.draw(36, 1, 0, 0);
    frame.draw_calls++;
}

}