#include "test_app_framework.hpp"

#include <stl/literals.hpp>
#include <iostream>
#include <imgui/imgui.h>

class SimpleExample : public ph::TestApplication {
private:
    ph::Mesh cube;
    ph::Texture texture;
    ph::Material material;
    ph::PointLight light;
public:
    SimpleExample() : ph::TestApplication(1280, 720, "Simple Example") {
        present->add_color_attachment("color1");
        present->add_color_attachment("color2");
        present->add_depth_attachment("depth1");
        present->add_depth_attachment("depth2");

        cube = generate_cube_geometry();
        texture = load_texture("data/textures/blank.png");
        material.texture = &texture;

        light.position = glm::vec3(0.0f, 2.0f, 4.0f);
        light.ambient = glm::vec3(34.0f / 255.0f, 34.0f / 255.0f, 34.0f / 255.0f);
        light.diffuse = glm::vec3(185.0f / 255.0f, 194.0f / 255.0f, 32.0f / 255.0f);
        light.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    }

    ~SimpleExample() = default;

    void update(ph::FrameInfo& frame, ph::RenderGraph& render_graph) override {
        // Note that we have to query these every frame if we want the size to update correctly.
        ph::RenderAttachment& color1 = present->get_attachment("color1");
        ph::RenderAttachment& color2 = present->get_attachment("color2");
        ph::RenderAttachment& depth1 = present->get_attachment("depth1");
        ph::RenderAttachment& depth2 = present->get_attachment("depth2");

        // Make ImGui UI
        if (ImGui::Begin("color1")) {
            ImVec2 const size = match_attachment_to_window_size(color1);
            // Note that we can discard the return value here
            match_attachment_to_window_size(depth1);

            ImGui::Image(color1.get_imgui_tex_id(), size);
        }
        ImGui::End();
        if (ImGui::Begin("color2")) {
            ImVec2 const size = match_attachment_to_window_size(color2);
            // Note that we can discard the return value here
            match_attachment_to_window_size(depth2);

            ImGui::Image(color2.get_imgui_tex_id(), size);
        }
        ImGui::End();
        if (ImGui::Begin("Renderer stats")) {
            ImGui::Text("Frametime: %f", frame_time);
            ImGui::End();
        }

        // Push info into render graph
        render_graph.materials.push_back(material);
        render_graph.point_lights.push_back(light);

        // Make renderpasses
        render_graph.projection = projection(glm::radians(45.0f), 0.1f, 100.0f, color1);
        render_graph.camera_pos = { 2.0f, 2.0f, 2.0f };
        render_graph.view = glm::lookAt(render_graph.camera_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        ph::RenderPass main_pass;
        main_pass.debug_name = "Main Pass";
        main_pass.outputs = { color1, depth1 };
        main_pass.clear_values = { clear_color, clear_depth };
        main_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &cube, 0 });
        main_pass.transforms.push_back(glm::scale(glm::mat4(1.0), glm::vec3(0.5, 0.5, 0.5)));
        main_pass.callback = [&frame, this](ph::CommandBuffer& cmd_buf) {
            renderer->execute_draw_commands(frame, cmd_buf);
        };
        render_graph.add_pass(stl::move(main_pass));

        ph::RenderPass second_pass;
        second_pass.debug_name = "Second Pass";
        second_pass.outputs = { color2, depth2 };
        second_pass.clear_values = { clear_color, clear_depth };
        second_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &cube, 0 });
        second_pass.transforms.push_back(glm::mat4(1.0f));
        second_pass.callback = [&frame, this](ph::CommandBuffer& cmd_buf) {
            renderer->execute_draw_commands(frame, cmd_buf);
        };
        render_graph.add_pass(stl::move(second_pass));
    }
};

class DeferredExample : public ph::TestApplication {
private:
    ph::Mesh cube;
    ph::Mesh quad;
    ph::Texture texture;
    ph::PointLight light;
public:
    DeferredExample() : ph::TestApplication(1280, 720, "Deferred") {
        present->add_color_attachment("position").resize(300, 300);
        present->add_color_attachment("normal").resize(300, 300);
        present->add_color_attachment("albedo_spec").resize(300, 300);
        present->add_color_attachment("deferred_final").resize(300, 300);
          
        cube = generate_cube_geometry();
        quad = generate_quad_geometry();
        texture = load_texture("data/textures/blank.png");

        create_deferred_pipeline();
        create_deferred_resolve_pipeline();

        light.position = glm::vec3(0.0f, 2.0f, 4.0f);
        light.ambient = glm::vec3(34.0f / 255.0f, 34.0f / 255.0f, 34.0f / 255.0f);
        light.diffuse = glm::vec3(185.0f / 255.0f, 194.0f / 255.0f, 32.0f / 255.0f);
        light.specular = glm::vec3(1.0f, 1.0f, 1.0f);
    }

    void create_deferred_pipeline() {
        using namespace stl::literals;
        ph::PipelineCreateInfo pci;

        vk::PipelineColorBlendAttachmentState blend;
        blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        blend.blendEnable = false;
        
        pci.blend_attachments.push_back(blend);
        pci.blend_attachments.push_back(blend);
        pci.blend_attachments.push_back(blend);

        pci.debug_name = "deferred";
        pci.dynamic_states.push_back(vk::DynamicState::eViewport);
        pci.dynamic_states.push_back(vk::DynamicState::eScissor);

        pci.viewports.emplace_back();
        pci.scissors.emplace_back();

        pci.vertex_input_binding.binding = 0;
        pci.vertex_input_binding.stride = 8 * sizeof(float);
        pci.vertex_input_binding.inputRate = vk::VertexInputRate::eVertex;
        pci.vertex_attributes.emplace_back(0_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 0_u32);
        pci.vertex_attributes.emplace_back(1_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 3 * (stl::uint32_t)sizeof(float));
        pci.vertex_attributes.emplace_back(2_u32, 0_u32, vk::Format::eR32G32Sfloat, 6 * (stl::uint32_t)sizeof(float));

        pci.shaders.emplace_back(ph::load_shader_code("data/shaders/deferred.vert.spv"), "main", vk::ShaderStageFlagBits::eVertex);
        pci.shaders.emplace_back(ph::load_shader_code("data/shaders/deferred.frag.spv"), "main", vk::ShaderStageFlagBits::eFragment);

        // Additional information can be reflected from the shaders
        ph::reflect_shaders(pci);
        
        ctx->pipelines.create_named_pipeline("deferred", stl::move(pci));
    }

    void create_deferred_resolve_pipeline() {
        using namespace stl::literals;
        ph::PipelineCreateInfo pci;

        vk::PipelineColorBlendAttachmentState blend;
        blend.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
        blend.blendEnable = false;

        pci.blend_attachments.push_back(blend);

        pci.debug_name = "deferred_resolve";
        pci.dynamic_states.push_back(vk::DynamicState::eViewport);
        pci.dynamic_states.push_back(vk::DynamicState::eScissor);

        pci.viewports.emplace_back();
        pci.scissors.emplace_back();

        pci.rasterizer.cullMode = vk::CullModeFlagBits::eNone;

        pci.vertex_input_binding.binding = 0;
        pci.vertex_input_binding.stride = 5 * sizeof(float);
        pci.vertex_input_binding.inputRate = vk::VertexInputRate::eVertex;
        pci.vertex_attributes.emplace_back(0_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 0_u32);
        pci.vertex_attributes.emplace_back(1_u32, 0_u32, vk::Format::eR32G32Sfloat, 3 * (stl::uint32_t)sizeof(float));

        pci.shaders.emplace_back(ph::load_shader_code("data/shaders/deferred_resolve.vert.spv"), "main", vk::ShaderStageFlagBits::eVertex);
        pci.shaders.emplace_back(ph::load_shader_code("data/shaders/deferred_resolve.frag.spv"), "main", vk::ShaderStageFlagBits::eFragment);

        // Additional information can be reflected from the shaders
        ph::reflect_shaders(pci);

        ctx->pipelines.create_named_pipeline("deferred_resolve", stl::move(pci));
    }

    vk::DescriptorSet get_deferred_descriptors(ph::ShaderInfo const& shader_info, ph::RenderGraph& graph, 
            ph::RenderPass& pass, ph::FrameInfo& frame) {

        ph::DescriptorSetBinding bindings;
        bindings.add(ph::make_descriptor(shader_info["camera"], frame.vp_ubo));
        bindings.add(ph::make_descriptor(shader_info["transforms"], frame.transform_ssbo.buffer_handle(), frame.transform_ssbo.size()));
        stl::vector<ph::ImageView> texture_views;
        texture_views.reserve(graph.materials.size());
        for (auto const& mat : graph.materials) { texture_views.push_back(mat.texture->view_handle()); }
        bindings.add(ph::make_descriptor(shader_info["textures"], texture_views, frame.default_sampler));

        // We need variable count to use descriptor indexing
        vk::DescriptorSetVariableDescriptorCountAllocateInfo variable_count_info;
        uint32_t counts[1]{ ph::meta::max_unbounded_array_size };
        variable_count_info.descriptorSetCount = 1;
        variable_count_info.pDescriptorCounts = counts;

        return renderer->get_descriptor(frame, pass, bindings, &variable_count_info);
    }

    vk::DescriptorSet get_deferred_resolve_descriptors(ph::ShaderInfo const& shader_info, ph::RenderGraph& graph,
        ph::RenderPass& pass, ph::FrameInfo& frame) {

        ph::RenderAttachment& position = present->get_attachment("position");
        ph::RenderAttachment& normal = present->get_attachment("normal");
        ph::RenderAttachment& albedo_spec = present->get_attachment("albedo_spec");

        ph::DescriptorSetBinding bindings;
        bindings.add(ph::make_descriptor(shader_info["gPosition"], position.image_view(), frame.default_sampler));
        bindings.add(ph::make_descriptor(shader_info["gNormal"], normal.image_view(), frame.default_sampler));
        bindings.add(ph::make_descriptor(shader_info["gAlbedoSpec"], albedo_spec.image_view(), frame.default_sampler));
        bindings.add(ph::make_descriptor(shader_info["lights"], frame.lights));
        bindings.add(ph::make_descriptor(shader_info["camera"], frame.vp_ubo));

        return renderer->get_descriptor(frame, pass, bindings);
    }

    ph::RenderPass deferred_main_pass(ph::FrameInfo& frame, ph::RenderGraph& render_graph) {
        ph::RenderAttachment& position = present->get_attachment("position");
        ph::RenderAttachment& normal = present->get_attachment("normal");
        ph::RenderAttachment& albedo_spec = present->get_attachment("albedo_spec");
        ph::RenderPass deferred;
        deferred.debug_name = "deferred pass";
        deferred.outputs = { position, normal, albedo_spec };
        deferred.clear_values = { clear_color, clear_color, clear_color };
        deferred.callback = [this, &frame, &render_graph](ph::CommandBuffer& cmd_buf) {
            vk::Rect2D scissor;
            scissor.offset = vk::Offset2D{ 0, 0 };
            scissor.extent = vk::Extent2D{ 300, 300 };
            vk::Viewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = 300;
            viewport.height = 300;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            cmd_buf.set_scissor(scissor);
            cmd_buf.set_viewport(viewport);

            ph::RenderPass& render_pass = *cmd_buf.get_active_renderpass();
            ph::Pipeline pipeline = renderer->get_pipeline("deferred", *cmd_buf.get_active_renderpass());
            cmd_buf.bind_pipeline(pipeline);

            ph::ShaderInfo const& shader_info = ctx->pipelines.get_named_pipeline("deferred")->shader_info;
            vk::DescriptorSet deferred_set = get_deferred_descriptors(shader_info, render_graph, render_pass, frame);

            // Update model matrix data
            glm::mat4 transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.5f));
            frame.transform_ssbo.write_data(deferred_set, &transform[0][0], sizeof(glm::mat4));

            cmd_buf.bind_descriptor_set(0, deferred_set);

            cmd_buf.bind_vertex_buffer(0, cube.get_vertices());
            cmd_buf.bind_index_buffer(cube.get_indices());

            // update push constant ranges
            stl::uint32_t const transform_index = 0;
            stl::uint32_t const material_index = 0;
            cmd_buf.push_constants(vk::ShaderStageFlagBits::eVertex, sizeof(uint32_t), sizeof(uint32_t), &transform_index);
            cmd_buf.push_constants(vk::ShaderStageFlagBits::eFragment, 0, sizeof(uint32_t), &material_index);

            cmd_buf.draw_indexed(cube.get_index_count(), 1);
        };

        return deferred;
    }

    ph::RenderPass deferred_resolve_pass(ph::FrameInfo& frame, ph::RenderGraph& render_graph) {
        ph::RenderAttachment& output = present->get_attachment("deferred_final");
        ph::RenderAttachment& position = present->get_attachment("position");
        ph::RenderAttachment& normal = present->get_attachment("normal");
        ph::RenderAttachment& albedo_spec = present->get_attachment("albedo_spec");

        ph::RenderPass pass;
        pass.debug_name = "deferred_resolve";
        pass.outputs = { output };
        pass.clear_values = { clear_color };
        pass.sampled_attachments = { position, normal, albedo_spec };
        pass.callback = [this, &render_graph, &frame](ph::CommandBuffer& cmd_buf) {
            vk::Rect2D scissor;
            scissor.offset = vk::Offset2D{ 0, 0 };
            scissor.extent = vk::Extent2D{ 300, 300 };
            vk::Viewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = 300;
            viewport.height = 300;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            cmd_buf.set_scissor(scissor);
            cmd_buf.set_viewport(viewport);

            ph::RenderPass& render_pass = *cmd_buf.get_active_renderpass();
            ph::Pipeline pipeline = renderer->get_pipeline("deferred_resolve", *cmd_buf.get_active_renderpass());
            cmd_buf.bind_pipeline(pipeline);

            ph::ShaderInfo const& shader_info = ctx->pipelines.get_named_pipeline("deferred_resolve")->shader_info;
            vk::DescriptorSet deferred_set = get_deferred_resolve_descriptors(shader_info, render_graph, render_pass, frame);

            cmd_buf.bind_descriptor_set(0, deferred_set);
            cmd_buf.bind_vertex_buffer(0, quad.get_vertices());
            cmd_buf.bind_index_buffer(quad.get_indices());
            cmd_buf.draw_indexed(quad.get_index_count(), 1);
        };

        return pass;
    }

    void update(ph::FrameInfo& frame, ph::RenderGraph& render_graph) override {
        ph::RenderAttachment& position = present->get_attachment("position");
        ph::RenderAttachment& normal = present->get_attachment("normal");
        ph::RenderAttachment& albedo_spec = present->get_attachment("albedo_spec");
        ph::RenderAttachment& output = present->get_attachment("deferred_final");
        // We will use the render graph to store our materials and lights
        render_graph.materials.push_back(ph::Material{ &texture });
        render_graph.point_lights.push_back(light);

        render_graph.projection = projection(glm::radians(45.0f), 0.1f, 100.0f, position);
        render_graph.camera_pos = glm::vec3(2, 2, 2);
        render_graph.view = glm::lookAt(render_graph.camera_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        render_graph.add_pass(deferred_main_pass(frame, render_graph));
        render_graph.add_pass(deferred_resolve_pass(frame, render_graph));

        // Simple debug visualization of the attachments
        if (ImGui::Begin("position")) {
            ImGui::Image(position.get_imgui_tex_id(), ImVec2(300, 300));
        }
        ImGui::End();

        if (ImGui::Begin("normal")) {
            ImGui::Image(normal.get_imgui_tex_id(), ImVec2(300, 300));
        }
        ImGui::End();

        if (ImGui::Begin("albedo_spec")) {
            ImGui::Image(albedo_spec.get_imgui_tex_id(), ImVec2(300, 300));
        }
        ImGui::End();

        if (ImGui::Begin("deferred_resolve")) {
            ImGui::Image(output.get_imgui_tex_id(), ImVec2(300, 300));
        }
        ImGui::End();
    }
};

int main() {
    DeferredExample app;
    app.run();
}