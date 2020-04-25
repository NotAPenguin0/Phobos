#include <phobos/fixed/skybox_renderer.hpp>

#include <phobos/fixed/util.hpp>
#include <stl/literals.hpp>

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
     1, 1, -1, 1, 1, 1, -1, 1, -1, -1, 1, 1
};

void SkyboxRenderer::create_pipeline(VulkanContext& ctx) {
    using namespace stl::literals;

    PipelineCreateInfo info;
    info.shaders.push_back(ph::create_shader(ctx, ph::load_shader_code("data/shaders/skybox.vert.spv"), "main", vk::ShaderStageFlagBits::eVertex));
    info.shaders.push_back(ph::create_shader(ctx, ph::load_shader_code("data/shaders/skybox.frag.spv"), "main", vk::ShaderStageFlagBits::eFragment));

    reflect_shaders(ctx, info);

    info.vertex_input_binding = vk::VertexInputBindingDescription(0, 3 * sizeof(float), vk::VertexInputRate::eVertex);
    info.vertex_attributes.emplace_back(0_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 0_u32);

    info.dynamic_states.push_back(vk::DynamicState::eViewport);
    info.dynamic_states.push_back(vk::DynamicState::eScissor);

    // Note that these are dynamic state so we don't need to fill in the fields
    info.viewports.emplace_back();
    info.scissors.emplace_back();

    info.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    info.depth_stencil.depthTestEnable = true;
    info.depth_stencil.depthWriteEnable = true;
    info.depth_stencil.depthBoundsTestEnable = false;
    // We want to render the skybox if the depth value is 1.0 (which it is), so we need to set LessOrEqual
    info.depth_stencil.depthCompareOp = vk::CompareOp::eLessOrEqual;

    vk::PipelineColorBlendAttachmentState blend_attachment;
    blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend_attachment.blendEnable = false;
    info.blend_attachments.push_back(blend_attachment);

    ctx.pipelines.create_named_pipeline("fixed_skybox", stl::move(info));

    ph::ShaderInfo const& shader_info = ctx.pipelines.get_named_pipeline("fixed_skybox")->shader_info;
    bindings.transform = shader_info["transform"];
    bindings.skybox = shader_info["skybox"];
}

SkyboxRenderer::SkyboxRenderer(ph::VulkanContext& ctx) {
    create_pipeline(ctx);
}

void SkyboxRenderer::build_render_pass(ph::FrameInfo& frame, ph::RenderAttachment& output, ph::RenderAttachment& depth, ph::RenderGraph& graph,
    ph::Renderer& renderer, CameraData const& camera) {

    if (!skybox) { return; }
    
    ph::RenderPass pass;
    pass.debug_name = "fixed_skybox";
    pass.outputs = { output, depth };
    // We do not want to clear anything, so leave the clearvalues vector empty
    pass.clear_values = {};
    pass.callback = [this, &camera, &renderer, &frame](ph::CommandBuffer& cmd_buf) {
        auto_viewport_scissor(cmd_buf);

        ph::RenderPass& pass = *cmd_buf.get_active_renderpass();

        ph::BufferSlice ubo = cmd_buf.allocate_scratch_ubo(sizeof(glm::mat4));
        std::memcpy(ubo.data, &camera.projection_mat[0][0], 16 * sizeof(float));

        ph::BufferSlice cube_mesh = cmd_buf.allocate_scratch_vbo(sizeof(skybox_vertices));
        std::memcpy(cube_mesh.data, skybox_vertices, sizeof(skybox_vertices));

        ph::Pipeline pipeline = renderer.get_pipeline("fixed_skybox", pass);
        cmd_buf.bind_pipeline(pipeline);

        DescriptorSetBinding set_binding;
        set_binding.add(ph::make_descriptor(bindings.transform, ubo));
        // TODO: Custom sampler?
        set_binding.add(ph::make_descriptor(bindings.skybox, skybox->view_handle(), frame.default_sampler));
        vk::DescriptorSet set = renderer.get_descriptor(frame, pass, set_binding);

        cmd_buf.bind_descriptor_set(0, set);
        cmd_buf.bind_vertex_buffer(0, cube_mesh);
        cmd_buf.draw(36, 1, 0, 0);
        frame.draw_calls++;
    };
    graph.add_pass(std::move(pass));
}

}

