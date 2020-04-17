#include "test_app_framework.hpp"

#include <stl/literals.hpp>
#include <iostream>
#include <imgui/imgui.h>

class SimpleExample : public ph::TestApplication {
private:
    ph::Mesh dragon;
    ph::Texture dragon_color;
    ph::Texture dragon_normal;

    ph::Mesh ground;
    ph::Texture ground_color;
    ph::Texture ground_normal;

    uint32_t dragon_material_idx;
    uint32_t ground_material_idx;
    
    ph::PointLight light;

    float dragon_rotation = 0;

public:
    SimpleExample() : ph::TestApplication(1280, 720, "Simple Example") {
        present->add_color_attachment("color1");
        present->add_depth_attachment("depth1");
        
        dragon = load_obj("data/dragon_scene/meshes/dragon.obj");
        dragon_color = load_texture("data/dragon_scene/textures/dragon_texture_color.png");
        dragon_normal = load_texture("data/dragon_scene/textures/dragon_texture_normal.png");

        ground = load_obj("data/dragon_scene/meshes/plane.obj");
        ground_color = load_texture("data/dragon_scene/textures/plane_texture_color.png");
        ground_normal = load_texture("data/dragon_scene/textures/plane_texture_normal.png");

        ph::Material dragon_material;
        dragon_material.diffuse = &dragon_color;
        dragon_material.normal = &dragon_normal;
        dragon_material_idx = add_material(dragon_material);

        ph::Material ground_material;
        ground_material.diffuse = &ground_color;
        ground_material.normal = &ground_normal;
        ground_material_idx = add_material(ground_material);

        light.position = glm::vec3(2, 2, 2);
        light.color = glm::vec3(1.0f, 1.0f, 1.0f);
        light.intensity = 1.0f;
    }

    ~SimpleExample() = default;

    void update(ph::FrameInfo& frame, ph::RenderGraph& render_graph) override {
        // Note that we have to query these every frame if we want the size to update correctly.
        ph::RenderAttachment& color1 = present->get_attachment("color1");
        ph::RenderAttachment& depth1 = present->get_attachment("depth1");

        // Make ImGui UI
        if (ImGui::Begin("color1")) {
            ImVec2 const size = match_attachment_to_window_size(color1);
            // Note that we can discard the return value here
            match_attachment_to_window_size(depth1);

            ImGui::Image(color1.get_imgui_tex_id(), size);
        }
        ImGui::End();

        dragon_rotation += frame_time * 6;

        // Push info into render graph
        render_graph.materials = materials;
        render_graph.point_lights.push_back(light);

        // Make renderpasses
        render_graph.projection = projection(glm::radians(45.0f), 0.1f, 100.0f, color1);
        render_graph.camera_pos = { 2.0f, 2.0f, 2.0f };
        render_graph.view = glm::lookAt(render_graph.camera_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        glm::mat4 dragon_transform = glm::scale(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, 0.5f));
        dragon_transform = glm::rotate(dragon_transform, glm::radians(dragon_rotation), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 floor_transform = glm::mat4(1.0f);
        floor_transform = glm::translate(floor_transform, glm::vec3(0, -0.5, 0));
        floor_transform = glm::rotate(floor_transform, glm::radians(30.0f), glm::vec3(1.0f, 0, 0));

        ph::RenderPass main_pass;
        main_pass.debug_name = "Main Pass";
        main_pass.outputs = { color1, depth1 };
        main_pass.clear_values = { vk::ClearColorValue{std::array<float, 4>{{0.427f, 0.427f, 0.427f, 1.0f}}}, clear_depth };
        main_pass.transforms.push_back(dragon_transform);
        main_pass.transforms.push_back(floor_transform);
        main_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &dragon, dragon_material_idx });
        main_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &ground, ground_material_idx });
        main_pass.callback = [&frame, this](ph::CommandBuffer& cmd_buf) {
            renderer->execute_draw_commands(frame, cmd_buf);
        };
        render_graph.add_pass(stl::move(main_pass));
    }
};

int main() {
    SimpleExample app;
    app.run();
}