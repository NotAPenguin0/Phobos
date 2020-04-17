#include "test_app_framework.hpp"

#include <stl/literals.hpp>
#include <iostream>
#include <imgui/imgui.h>

class SimpleExample : public ph::TestApplication {
private:
    ph::Mesh cube;
    ph::Mesh ground;

    ph::Texture texture;
    ph::Texture specular_map;

    ph::Texture cube_diffuse;
    ph::Texture cube_specular;
    ph::Texture cube_normal;

    ph::Material material;
    ph::Material cube_material;

    ph::DirectionalLight light;
public:
    SimpleExample() : ph::TestApplication(1280, 720, "Simple Example") {
        present->add_color_attachment("color1");
        present->add_depth_attachment("depth1");
        
        cube = generate_cube_geometry();
        ground = generate_plane_geometry();
        texture = load_texture("data/textures/container.png");
        specular_map = load_texture("data/textures/spec.png");
        material.diffuse = &texture;
        material.specular = &specular_map;

        cube_diffuse = load_texture("data/textures/bricks_color.jpg");
        cube_specular = load_texture("data/textures/bricks_specular.jpg");
        cube_normal = load_texture("data/textures/bricks_normal.jpg");

        cube_material.diffuse = &cube_diffuse;
        cube_material.specular = &cube_specular;
        cube_material.normal = &cube_normal;
          
        light.direction = glm::vec3(0.0f, -1.0f, 0.0f);
        light.color = glm::vec3(1.0f, 1.0f, 1.0f);
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

        // Update light position to rotate the direction around
        light.direction.x = std::sin(time);
        light.direction.z = std::cos(time);

        // Push info into render graph
        render_graph.materials.push_back(material);
        render_graph.materials.push_back(cube_material);
        render_graph.directional_lights.push_back(light);

        // Make renderpasses
        render_graph.projection = projection(glm::radians(45.0f), 0.1f, 100.0f, color1);
        render_graph.camera_pos = { 2.0f, 2.0f, 2.0f };
        render_graph.view = glm::lookAt(render_graph.camera_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

        ph::RenderPass main_pass;
        main_pass.debug_name = "Main Pass";
        main_pass.outputs = { color1, depth1 };
        main_pass.clear_values = { vk::ClearColorValue{std::array<float, 4>{{0.427f, 0.427f, 0.427f, 1.0f}}}, clear_depth };
        main_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &cube, 1 });
        main_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &ground, 0 });
        main_pass.transforms.push_back(glm::scale(glm::mat4(1.0), glm::vec3(0.5, 0.5, 0.5)));
        main_pass.transforms.push_back(
            glm::rotate(
                glm::translate(glm::mat4(1.0), glm::vec3(0.0f, -0.5f, 0.0f)), 
                glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f))
        );
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