#include <phobos/fixed/lighting_pass.hpp>
#include <phobos/fixed/util.hpp>

#include <stl/literals.hpp>

#include <glm/gtc/type_ptr.hpp>

namespace ph::fixed {

void LightingPass::create_pipeline(ph::VulkanContext& ctx) {
    using namespace stl::literals;

    ph::PipelineCreateInfo pci;

    // We need to blend the resulting light values together
    vk::PipelineColorBlendAttachmentState blend;
    blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend.blendEnable = true;
    // Additive blending
    blend.colorBlendOp = vk::BlendOp::eAdd;
    blend.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
    blend.dstColorBlendFactor = vk::BlendFactor::eOne;
    blend.alphaBlendOp = vk::BlendOp::eAdd;
    blend.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    blend.dstAlphaBlendFactor = vk::BlendFactor::eOne;

    pci.blend_attachments.push_back(blend);

    pci.debug_name = "fixed_deferred_lighting"; // #Tag(Release)

    // To avoid having to recreate the pipeline when the viewport is changed
    pci.dynamic_states.push_back(vk::DynamicState::eViewport);
    pci.dynamic_states.push_back(vk::DynamicState::eScissor);
    // Even though they are dynamic states, viewportCount must be 1 if the multiple viewports feature is  not enabled. The pViewports field
    // is ignored though, so the actual values don't matter. 
    // See  also https://renderdoc.org/vkspec_chunked/chap25.html#VkPipelineViewportStateCreateInfo
    pci.viewports.emplace_back();
    pci.scissors.emplace_back();

    // The lighting pass will render light geometry. We don't need to shade this light geometry, so only a vec3 for the position is enough.
    // Note that we will calculate screenspace texture coordinates to sample the Gbuffer in the vertex shader.
    constexpr size_t stride = 3 * sizeof(float);
    pci.vertex_input_binding = vk::VertexInputBindingDescription(0, stride, vk::VertexInputRate::eVertex);
    pci.vertex_attributes.emplace_back(0_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 0_u32);

    // Disable depth testing for deferred resolve pipeline, since we're drawing a fullscreen quad
    pci.depth_stencil.depthTestEnable = false;
    pci.depth_stencil.depthWriteEnable = false;
    // If we do not cull any geometry in the lighting pass, all lights will have double the intensity (since front and back faces are both 
    // rendered). The problem with culling backfaces arrives when the camera is inside a light volume: That light volume would no longer be
    // shaded since all back faces are culled. To fix this, we have to cull front faces instead.
    pci.rasterizer.cullMode = vk::CullModeFlagBits::eFront;

    std::vector<uint32_t> vert_code = ph::load_shader_code("data/shaders/deferred_lighting.vert.spv");
    std::vector<uint32_t> frag_code = ph::load_shader_code("data/shaders/deferred_lighting.frag.spv");

    ph::ShaderHandle vertex_shader = ph::create_shader(ctx, vert_code, "main", vk::ShaderStageFlagBits::eVertex);
    ph::ShaderHandle fragment_shader = ph::create_shader(ctx, frag_code, "main", vk::ShaderStageFlagBits::eFragment);

    pci.shaders.push_back(vertex_shader);
    pci.shaders.push_back(fragment_shader);

    ph::reflect_shaders(ctx, pci);
    // Store bindings so we don't need to look them up every frame
    bindings.depth = pci.shader_info["gDepth"];
    bindings.normal = pci.shader_info["gNormal"];
    bindings.albedo_spec = pci.shader_info["gAlbedoSpec"];
    bindings.lights = pci.shader_info["lights"];
    bindings.camera = pci.shader_info["camera"];

    ctx.pipelines.create_named_pipeline("fixed_deferred_lighting", std::move(pci));
}

// Exported from blender
static constexpr float light_volume_geometry[] = {
    0.000000, -1.000000, 0.000000,
    0.723607, -0.447220, 0.525725,
    -0.276388, -0.447220, 0.85064,
    -0.894426, -0.447216, 0.00000,
    -0.276388, -0.447220, -0.8506,
    0.723607, -0.447220, -0.52572,
    0.276388, 0.447220, 0.850649,
    -0.723607, 0.447220, 0.525725,
    -0.723607, 0.447220, -0.52572,
    0.276388, 0.447220, -0.850649,
    0.894426, 0.447216, 0.000000,
    0.000000, 1.000000, 0.000000,
    -0.162456, -0.850654, 0.49999,
    0.425323, -0.850654, 0.309011,
    0.262869, -0.525738, 0.809012,
    0.850648, -0.525736, 0.000000,
    0.425323, -0.850654, -0.30901,
    -0.525730, -0.850652, 0.00000,
    -0.688189, -0.525736, 0.49999,
    -0.162456, -0.850654, -0.4999,
    -0.688189, -0.525736, -0.4999,
    0.262869, -0.525738, -0.80901,
    0.951058, 0.000000, 0.309013,
    0.951058, 0.000000, -0.309013,
    0.000000, 0.000000, 1.000000,
    0.587786, 0.000000, 0.809017,
    -0.951058, 0.000000, 0.309013,
    -0.587786, 0.000000, 0.809017,
    -0.587786, 0.000000, -0.80901,
    -0.951058, 0.000000, -0.30901,
    0.587786, 0.000000, -0.809017,
    0.000000, 0.000000, -1.000000,
    0.688189, 0.525736, 0.499997,
    -0.262869, 0.525738, 0.809012,
    -0.850648, 0.525736, 0.000000,
    -0.262869, 0.525738, -0.80901,
    0.688189, 0.525736, -0.499997,
    0.162456, 0.850654, 0.499995,
    0.525730, 0.850652, 0.000000,
    -0.425323, 0.850654, 0.309011,
    -0.425323, 0.850654, -0.30901,
    0.162456, 0.850654, -0.499995
};
static constexpr uint32_t light_volume_indices[] = {
    0 , 13 , 12 , 1 , 13 , 15 , 0 , 12 , 17 , 0 , 17 , 19 , 0 , 19 , 16 , 1 ,
 15 , 22 , 2 , 14 , 24 , 3 , 18 , 26 , 4 , 20 , 28 , 5 , 21 , 30 , 1 , 22
 , 25 , 2 , 24 , 27 , 3 , 26 , 29 , 4 , 28 , 31 , 5 , 30 , 23 , 6 , 32 ,
37 , 7 , 33 , 39 , 8 , 34 , 40 , 9 , 35 , 41 , 10 , 36 , 38 , 38 , 41 , 11 , 
38 , 36 , 41 , 36 , 9 , 41 , 41 , 40 , 11 , 41 , 35 , 40 , 35 , 8 , 40 , 
40 , 39 , 11 , 40 , 34 , 39 , 34 , 7 , 39 , 39 , 37 , 11 , 39 , 33 ,
37 , 33 , 6 , 37 , 37 , 38 , 11 , 37 , 32 , 38 , 32 , 10 , 38 , 23 , 36 ,
 10 , 23 , 30 , 36 , 30 , 9 , 36 , 31 , 35 , 9 , 31 , 28 , 35 , 28 , 8 ,
35 , 29 , 34 , 8 , 29 , 26 , 34 , 26 , 7 , 34 , 27 , 33 , 7 , 27 , 24 , 33 , 
24 , 6 , 33 , 25 , 32 , 6 , 25 , 22 , 32 , 22 , 10 , 32 , 30 , 31 , 9
 , 30 , 21 , 31 , 21 , 4 , 31 , 28 , 29 , 8 , 28 , 20 , 29 , 20 , 3 , 29
, 26 , 27 , 7 , 26 , 18 , 27 , 18 , 2 , 27 , 24 , 25 , 6 , 24 , 14 , 25 ,
 14 , 1 , 25 , 22 , 23 , 10 , 22 , 15 , 23 , 15 , 5 , 23 , 16 , 21 , 5 ,
16 , 19 , 21 , 19 , 4 , 21 , 19 , 20 , 4 , 19 , 17 , 20 , 17 , 3 , 20 , 17 , 
18 , 3 , 17 , 12 , 18 , 12 , 2 , 18 , 15 , 16 , 5 , 15 , 13 , 16 , 13
 , 0 , 16 , 12 , 14 , 2 , 12 , 13 , 14 , 13 , 1 , 14 ,
};

void LightingPass::create_light_volume_mesh(ph::VulkanContext& ctx) {
    // I modeled an icosahedron in blender for this. We could use a more accurate shape, but I think this comes close enough to the 
    // sphere representation that we need, without causing too much overhead.

    ph::Mesh::CreateInfo info;

    info.ctx = &ctx;
    info.vertex_size = 3; // We only have positions
    info.vertex_count = sizeof(light_volume_geometry) / (3 * sizeof(float));
    info.vertices = light_volume_geometry;
    info.index_count = sizeof(light_volume_indices) / sizeof(uint32_t);
    info.indices = light_volume_indices;

    light_volume.create(info);
}

LightingPass::LightingPass(ph::VulkanContext& ctx) {
    create_pipeline(ctx);
    create_light_volume_mesh(ctx);
}

void LightingPass::frame_end() {
    point_lights.clear();
    per_frame_buffers = {};
}

void LightingPass::add_point_light(Projection projection, ph::PointLight const& light) {
    // TODO: Implement the frustum test
    point_lights.push_back(light);
}

void LightingPass::build_render_pass(ph::FrameInfo& frame, ph::RenderAttachment& output, ph::RenderAttachment& depth, ph::RenderAttachment& normal,
	ph::RenderAttachment& albedo_spec, ph::RenderGraph& graph, ph::Renderer& renderer, CameraData const& camera) {

    ph::RenderPass pass;

    pass.debug_name = "fixed_deferred_lighting_pass";
    pass.outputs = { output };
    pass.sampled_attachments = { normal, depth, albedo_spec };
    pass.clear_values = { vk::ClearColorValue{ std::array<float, 4>{ {0.0f, 0.0f, 0.0f, 1.0f}} } };

    pass.callback = [this, &frame, &renderer, &camera, &normal, &depth, &albedo_spec](ph::CommandBuffer& cmd_buf) {
        auto_viewport_scissor(cmd_buf);

        // Don't do anything if there aren't any lights to render
        if (point_lights.empty()) { return; }

        update_camera(cmd_buf, camera);
        update_lights(cmd_buf);

        ph::RenderPass& pass = *cmd_buf.get_active_renderpass();
        ph::Pipeline pipeline = renderer.get_pipeline("fixed_deferred_lighting", pass);
        cmd_buf.bind_pipeline(pipeline);

        ph::DescriptorSetBinding set;
        set.add(ph::make_descriptor(bindings.camera, per_frame_buffers.camera));
        set.add(ph::make_descriptor(bindings.lights, per_frame_buffers.lights));
        set.add(ph::make_descriptor(bindings.depth, depth.image_view(), frame.default_sampler));
        set.add(ph::make_descriptor(bindings.albedo_spec, albedo_spec.image_view(), frame.default_sampler));
        set.add(ph::make_descriptor(bindings.normal, normal.image_view(), frame.default_sampler));
        vk::DescriptorSet descr_set = renderer.get_descriptor(frame, pass, set);
        cmd_buf.bind_descriptor_set(0, descr_set);

        cmd_buf.bind_vertex_buffer(0, light_volume.get_vertices());
        cmd_buf.bind_index_buffer(light_volume.get_indices());

        for (size_t i = 0; i < point_lights.size(); ++i) {
            // Push light index
            cmd_buf.push_constants(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0, sizeof(uint32_t), &i);
            cmd_buf.draw_indexed(light_volume.get_index_count(), 1, 0, 0, 0);
            frame.draw_calls++;
        }
    };

    graph.add_pass(stl::move(pass));
}

void LightingPass::update_camera(ph::CommandBuffer& cmd_buf, CameraData const& camera) {
    // projection inverse_projection, inverse_view and camera pos
    vk::DeviceSize const size = 3 * sizeof(glm::mat4) + sizeof(glm::vec4);
    ph::BufferSlice ubo = cmd_buf.allocate_scratch_ubo(size);
    
    std::memcpy(ubo.data, glm::value_ptr(camera.projection_view), sizeof(glm::mat4));

    glm::mat4 inverse_projection = glm::inverse(camera.projection_mat);
    glm::mat4 inverse_view = glm::inverse(camera.view);

    std::memcpy(ubo.data + sizeof(glm::mat4), glm::value_ptr(inverse_projection), sizeof(glm::mat4));
    std::memcpy(ubo.data + 2 * sizeof(glm::mat4), glm::value_ptr(inverse_view), sizeof(glm::mat4));
    std::memcpy(ubo.data + 3 * sizeof(glm::mat4), glm::value_ptr(camera.position), sizeof(glm::vec3));

    per_frame_buffers.camera = ubo;
}

void LightingPass::update_lights(ph::CommandBuffer& cmd_buf) {
    vk::DeviceSize const size = sizeof(ph::PointLight) * point_lights.size();
    ph::BufferSlice buffer = cmd_buf.allocate_scratch_ssbo(size);

    std::memcpy(buffer.data, point_lights.data(), size);

    per_frame_buffers.lights = buffer;
}

}
