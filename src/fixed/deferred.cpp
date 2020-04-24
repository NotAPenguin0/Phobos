#include <phobos/fixed/deferred.hpp>
#include <phobos/fixed/util.hpp>

#include <phobos/pipeline/pipeline.hpp>
#include <stl/literals.hpp>

#include <glm/gtc/type_ptr.hpp>

namespace ph::fixed {

void DeferredRenderer::create_main_pipeline(ph::VulkanContext& ctx) {
    using namespace stl::literals;

	ph::PipelineCreateInfo pci;

    // We don't do blending in the deferred pipeline, so we need to disable it for all color components
    vk::PipelineColorBlendAttachmentState blend;
    blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend.blendEnable = false;

    // Since we have two color attachments, we need to add the blend info twice
    pci.blend_attachments.push_back(blend);
    pci.blend_attachments.push_back(blend);

    pci.debug_name = "fixed_deferred_main"; // #Tag(Release)
    
    // To avoid having to recreate the pipeline when the viewport is changed
    pci.dynamic_states.push_back(vk::DynamicState::eViewport);
    pci.dynamic_states.push_back(vk::DynamicState::eScissor);
    // Even though they are dynamic states, viewportCount must be 1 if the multiple viewports feature is  not enabled. The pViewports field
    // is ignored though, so the actual values don't matter. 
    // See  also https://renderdoc.org/vkspec_chunked/chap25.html#VkPipelineViewportStateCreateInfo
    pci.viewports.emplace_back();
    pci.scissors.emplace_back();

    // Position + Normal + Tangent + TexCoord
    constexpr size_t stride = (3 + 3 + 3 + 2) * sizeof(float);
    pci.vertex_input_binding = vk::VertexInputBindingDescription(0, stride, vk::VertexInputRate::eVertex);
    // vec3 iPos;
    pci.vertex_attributes.emplace_back(0_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 0_u32);
    // vec3 iNormal
    pci.vertex_attributes.emplace_back(1_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 3 * (stl::uint32_t)sizeof(float));
    // vec3 iTangent;
    pci.vertex_attributes.emplace_back(2_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 6 * (stl::uint32_t)sizeof(float));
    // vec2 iTexCoords
    pci.vertex_attributes.emplace_back(3_u32, 0_u32, vk::Format::eR32G32Sfloat, 9 * (stl::uint32_t)sizeof(float));

    std::vector<uint32_t> vert_code = ph::load_shader_code("data/shaders/deferred.vert.spv");
    std::vector<uint32_t> frag_code = ph::load_shader_code("data/shaders/deferred.frag.spv");

    ph::ShaderHandle vertex_shader = ph::create_shader(ctx, vert_code, "main", vk::ShaderStageFlagBits::eVertex);
    ph::ShaderHandle fragment_shader = ph::create_shader(ctx, frag_code, "main", vk::ShaderStageFlagBits::eFragment);

    pci.shaders.push_back(vertex_shader);
    pci.shaders.push_back(fragment_shader);

    ph::reflect_shaders(ctx, pci);
    // Store bindings so we don't need to look them up every frame
    main_pass_bindings.camera = pci.shader_info["camera"];
    main_pass_bindings.transforms = pci.shader_info["transforms"];
    main_pass_bindings.textures = pci.shader_info["textures"];

    ctx.pipelines.create_named_pipeline("fixed_deferred_main", std::move(pci));
}

void DeferredRenderer::create_resolve_pipeline(ph::VulkanContext& ctx) {
    using namespace stl::literals;

    ph::PipelineCreateInfo pci;

    // We don't need any blending for the resolve pass
    vk::PipelineColorBlendAttachmentState blend;
    blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend.blendEnable = false;

    pci.blend_attachments.push_back(blend);

    pci.debug_name = "fixed_deferred_resolve"; // #Tag(Release)

    // To avoid having to recreate the pipeline when the viewport is changed
    pci.dynamic_states.push_back(vk::DynamicState::eViewport);
    pci.dynamic_states.push_back(vk::DynamicState::eScissor);
    // Even though they are dynamic states, viewportCount must be 1 if the multiple viewports feature is  not enabled. The pViewports field
    // is ignored though, so the actual values don't matter. 
    // See  also https://renderdoc.org/vkspec_chunked/chap25.html#VkPipelineViewportStateCreateInfo
    pci.viewports.emplace_back();
    pci.scissors.emplace_back();

    // The resolve pass renders a single fullscreen quad, so we only need to pass a vec2 for the position and a vec2 for texcoords
    constexpr size_t stride = (2 + 2) * sizeof(float);
    pci.vertex_input_binding = vk::VertexInputBindingDescription(0, stride, vk::VertexInputRate::eVertex);
    // vec2 iPos;
    pci.vertex_attributes.emplace_back(0_u32, 0_u32, vk::Format::eR32G32Sfloat, 0_u32);
    // vec2 iPos;
    pci.vertex_attributes.emplace_back(1_u32, 0_u32, vk::Format::eR32G32Sfloat, (uint32_t)(2 * sizeof(float)));

    // Disable depth testing for deferred resolve pipeline, since we're drawing a fullscreen quad
    pci.depth_stencil.depthTestEnable = false;
    pci.depth_stencil.depthWriteEnable = false;
    pci.rasterizer.cullMode = vk::CullModeFlagBits::eNone;
    
    std::vector<uint32_t> vert_code = ph::load_shader_code("data/shaders/deferred_resolve.vert.spv");
    std::vector<uint32_t> frag_code = ph::load_shader_code("data/shaders/deferred_resolve.frag.spv");

    ph::ShaderHandle vertex_shader = ph::create_shader(ctx, vert_code, "main", vk::ShaderStageFlagBits::eVertex);
    ph::ShaderHandle fragment_shader = ph::create_shader(ctx, frag_code, "main", vk::ShaderStageFlagBits::eFragment);

    pci.shaders.push_back(vertex_shader);
    pci.shaders.push_back(fragment_shader);

    ph::reflect_shaders(ctx, pci);
    // Store bindings so we don't need to look them up every frame
    resolve_pass_bindings.depth = pci.shader_info["gDepth"];
    resolve_pass_bindings.normal = pci.shader_info["gNormal"];
    resolve_pass_bindings.albedo_spec = pci.shader_info["gAlbedoSpec"];
    resolve_pass_bindings.lights = pci.shader_info["lights"];
    resolve_pass_bindings.camera = pci.shader_info["camera"];

    ctx.pipelines.create_named_pipeline("fixed_deferred_resolve", std::move(pci));
}

DeferredRenderer::DeferredRenderer(ph::VulkanContext& ctx, ph::PresentManager& present, vk::Extent2D resolution)
    : skybox(ctx) {
	depth = &present.add_depth_attachment("fixed_depth", resolution);
	normal = &present.add_color_attachment("fixed_normal", resolution, vk::Format::eR16G16B16A16Unorm);
	albedo_spec = &present.add_color_attachment("fixed_albedo_spec", resolution, vk::Format::eR8G8B8A8Unorm);

    create_main_pipeline(ctx);
    create_resolve_pipeline(ctx);
}

void DeferredRenderer::frame_end() {
    draws.clear();
    materials.clear();
    transforms.clear();
    skybox.set_skybox(nullptr);
    per_frame_resources = {};
}

void DeferredRenderer::resize(vk::Extent2D size) {
    depth->resize(size.width, size.height);
    normal->resize(size.width, size.height);
    albedo_spec->resize(size.width, size.height);
}

void DeferredRenderer::add_draw(ph::Mesh& mesh, uint32_t material_index, glm::mat4 const& transform) {
    draws.push_back({ &mesh, material_index, (uint32_t)transforms.size() });
    transforms.push_back(transform);
}

uint32_t DeferredRenderer::add_material(ph::Material const& material) {
    materials.push_back(material);
    return materials.size() - 1;
}

void DeferredRenderer::set_skybox(ph::Cubemap* sb) {
    skybox.set_skybox(sb);
}

// Builds the main renderpass to do deferred rendering. The output attachment must be a valid color attachment.
void DeferredRenderer::build_main_pass(ph::FrameInfo& frame, ph::RenderGraph& graph, ph::Renderer& renderer) {
    ph::RenderPass pass;
    pass.debug_name = "fixed_deferred_main_pass"; // #Tag(Release)
    pass.outputs = { *normal, *albedo_spec, *depth };
    pass.sampled_attachments = {};
    vk::ClearValue clear_color = vk::ClearColorValue{ std::array<float, 4>{ {0.0f, 0.0f, 0.0f, 1.0f}} };
    vk::ClearValue clear_depth = vk::ClearDepthStencilValue{ 1.0f, 0 };
    pass.clear_values = { clear_color, clear_color, clear_depth };

    pass.callback = [this, &frame, &renderer, &graph](ph::CommandBuffer& cmd_buf) {
        // Setup automatic viewport because we just want to cover the full output attachment
        auto_viewport_scissor(cmd_buf);

        // Note that we must set the viewport and scissor state before early returning, this is because we specified them as dynamic states.
        // This requires us to have commands to update them.
        if (draws.empty()) return;

        // Bind the pipeline
        ph::RenderPass& pass = *cmd_buf.get_active_renderpass();
        ph::Pipeline pipeline = renderer.get_pipeline("fixed_deferred_main", pass);
        cmd_buf.bind_pipeline(pipeline);

        update_transforms(cmd_buf);

        // Bind descriptor set
        vk::DescriptorSet descr_set = get_main_pass_descriptors(frame, renderer, pass);
        cmd_buf.bind_descriptor_set(0, descr_set);

        for (auto& draw : draws) {
            ph::Mesh* mesh = draw.mesh;
            // Bind draw data
            cmd_buf.bind_vertex_buffer(0, mesh->get_vertices());
            cmd_buf.bind_index_buffer(mesh->get_indices());
            
            // update push constant ranges
            stl::uint32_t const transform_index = draw.transform_index;
            cmd_buf.push_constants(vk::ShaderStageFlagBits::eVertex, 0, sizeof(uint32_t), &transform_index);
            // First texture is diffuse, second is specular, third is normal. See also get_fixed_descriptor_set() where we fill the textures array
            stl::uint32_t const texture_indices[] = { 3 * draw.material_index, 3 * draw.material_index + 1, 3 * draw.material_index + 2 };
            cmd_buf.push_constants(vk::ShaderStageFlagBits::eFragment, sizeof(uint32_t), sizeof(texture_indices), &texture_indices);
            // Execute drawcall
            cmd_buf.draw_indexed(mesh->get_index_count(), 1, 0, 0, 0);
            frame.draw_calls++;
        }
    };

    graph.add_pass(stl::move(pass));
}

static constexpr float quad_geometry[] = {
    -1, 1, 0, 1, -1, -1, 0, 0,
    1, -1, 1, 0, -1, 1, 0, 1,
    1, -1, 1, 0, 1, 1, 1, 1
};

void DeferredRenderer::build_resolve_pass(ph::FrameInfo& frame, ph::RenderAttachment& output, ph::RenderGraph& graph, ph::Renderer& renderer) {
    ph::RenderPass pass;

    pass.debug_name = "fixed_deferred_resolve_pass";
    pass.outputs = { output };
    pass.sampled_attachments = { *normal, *depth, *albedo_spec };
    pass.clear_values = { vk::ClearColorValue{ std::array<float, 4>{ {0.0f, 0.0f, 0.0f, 1.0f}} } };

    pass.callback = [this, &frame, &renderer, &graph](ph::CommandBuffer& cmd_buf) {
        // TODO: Allow specifying viewport
        auto_viewport_scissor(cmd_buf);

        ph::RenderPass& pass = *cmd_buf.get_active_renderpass();
        ph::Pipeline pipeline = renderer.get_pipeline("fixed_deferred_resolve", pass);
        cmd_buf.bind_pipeline(pipeline);

        update_camera_data(cmd_buf, graph);

        vk::DescriptorSet descr_set = get_resolve_pass_descriptors(frame, renderer, pass);
        cmd_buf.bind_descriptor_set(0, descr_set);

        ph::BufferSlice geometry = cmd_buf.allocate_scratch_vbo(sizeof(quad_geometry));
        std::memcpy(geometry.data, quad_geometry, sizeof(quad_geometry));
        cmd_buf.bind_vertex_buffer(0, geometry);

        cmd_buf.draw(6, 1, 0, 0);
        frame.draw_calls++;
    };

    graph.add_pass(stl::move(pass));
}

void DeferredRenderer::build_all(ph::FrameInfo& frame, ph::RenderAttachment& output, ph::RenderGraph& graph, ph::Renderer& renderer) {
    build_main_pass(frame, graph, renderer);
    build_resolve_pass(frame, output, graph, renderer);
    skybox.build_render_pass(frame, output, *depth, graph, renderer);
}

void DeferredRenderer::update_transforms(ph::CommandBuffer& cmd_buf) {
    vk::DeviceSize const size = transforms.size() * sizeof(glm::mat4);
    per_frame_resources.transforms = cmd_buf.allocate_scratch_ssbo(size);
    std::memcpy(per_frame_resources.transforms.data, transforms.data(), size);
}

void DeferredRenderer::update_camera_data(ph::CommandBuffer& cmd_buf, ph::RenderGraph& graph) {
    // We need 2 matrices (inverse-projection and inverse-view), and a vec3 (padded to vec4) for the camera position.
    vk::DeviceSize const size = 2 * sizeof(glm::mat4) + sizeof(glm::vec4);
    per_frame_resources.camera = cmd_buf.allocate_scratch_ubo(size);
    
    // Calculate matrices and send over
    glm::mat4 inverse_projection = glm::inverse(graph.projection);
    glm::mat4 inverse_view = glm::inverse(graph.view);
    std::memcpy(per_frame_resources.camera.data, glm::value_ptr(inverse_projection), sizeof(glm::mat4));
    std::memcpy(per_frame_resources.camera.data + sizeof(glm::mat4), glm::value_ptr(inverse_view), sizeof(glm::mat4));
    std::memcpy(per_frame_resources.camera.data + 2 * sizeof(glm::mat4), glm::value_ptr(graph.camera_pos), sizeof(glm::vec3));
}

vk::DescriptorSet DeferredRenderer::get_main_pass_descriptors(ph::FrameInfo& frame, ph::Renderer& renderer, ph::RenderPass& pass) {
    ph::BuiltinUniforms ubos = renderer.get_builtin_uniforms();
    ph::DefaultTextures& default_textures = renderer.get_default_textures();

    ph::DescriptorSetBinding bindings;
    bindings.add(ph::make_descriptor(main_pass_bindings.camera, ubos.camera));

    stl::vector<ImageView> texture_views;
    texture_views.reserve(materials.size() * 4);
    for (auto const& mat : materials) {
        auto push_or_default = [&texture_views](ph::Texture* tex, ph::Texture& def) {
            if (tex) { texture_views.push_back(tex->view_handle()); }
            else { texture_views.push_back(def.view_handle()); }
        };

        push_or_default(mat.diffuse, default_textures.color);
        push_or_default(mat.specular, default_textures.specular);
        push_or_default(mat.normal, default_textures.normal);
    }

    bindings.add(ph::make_descriptor(main_pass_bindings.textures, texture_views, frame.default_sampler));
    bindings.add(ph::make_descriptor(main_pass_bindings.transforms, per_frame_resources.transforms));

    // We need variable count to use descriptor indexing
    vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
    uint32_t counts[1]{ meta::max_unbounded_array_size };
    variable_count_info.descriptorSetCount = 1;
    variable_count_info.pDescriptorCounts = counts;

    return renderer.get_descriptor(frame, pass, bindings, &variable_count_info);
}

vk::DescriptorSet DeferredRenderer::get_resolve_pass_descriptors(ph::FrameInfo& frame, ph::Renderer& renderer, ph::RenderPass& pass) {
    ph::BuiltinUniforms ubos = renderer.get_builtin_uniforms();
    ph::DescriptorSetBinding bindings;
    bindings.add(ph::make_descriptor(resolve_pass_bindings.normal, normal->image_view(), frame.default_sampler));
    bindings.add(ph::make_descriptor(resolve_pass_bindings.depth, depth->image_view(), frame.default_sampler));
    bindings.add(ph::make_descriptor(resolve_pass_bindings.albedo_spec, albedo_spec->image_view(), frame.default_sampler));
    bindings.add(ph::make_descriptor(resolve_pass_bindings.lights, ubos.lights));
    bindings.add(ph::make_descriptor(resolve_pass_bindings.camera, per_frame_resources.camera));
    return renderer.get_descriptor(frame, pass, bindings);
}

}
