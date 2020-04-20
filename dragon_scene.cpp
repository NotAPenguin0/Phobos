#include "test_app_framework.hpp"

#include <stl/literals.hpp>
#include <iostream>
#include <imgui/imgui.h>

#include <phobos/renderer/cubemap.hpp>

class DragonScene : public ph::TestApplication {
private:
	ph::Mesh dragon;
	ph::Texture dragon_normal;
	ph::Texture dragon_color;
	uint32_t dragon_material;
	glm::vec3 dragon_pos = glm::vec3(0, 0.8, 0);

	ph::Mesh ground;
	ph::Texture ground_normal;
	ph::Texture ground_color;
	uint32_t ground_material;

	ph::Mesh monkey;
	ph::Texture monkey_normal;
	ph::Texture monkey_color;
	uint32_t monkey_material;
	glm::vec3 monkey_pos = glm::vec3(1.4f, 2.0f, -0.9f);
	float monkey_rotation = 0.0f;

	ph::PointLight light;
	
	ph::Cubemap skybox;
public:
	void load_resources() {
		dragon = load_obj("data/dragon_scene/meshes/dragon.obj");
		// Make sure to call load_texture_map() for non-color textures or Srgb encoding will haunt you forever
		dragon_normal = load_texture_map("data/dragon_scene/textures/dragon_texture_normal.png");
		dragon_color = load_texture("data/dragon_scene/textures/dragon_texture_color.png");

		ph::Material dragon_mat{ &dragon_color, nullptr, &dragon_normal };
		dragon_material = add_material(dragon_mat);

		ground = load_obj("data/dragon_scene/meshes/plane.obj");
		ground_normal = load_texture_map("data/dragon_scene/textures/plane_texture_normal.png");
		ground_color = load_texture("data/dragon_scene/textures/plane_texture_color.png");
		
		ph::Material ground_mat{ &ground_color, nullptr, &ground_normal };
		ground_material = add_material(ground_mat);


		monkey = load_obj("data/dragon_scene/meshes/suzanne.obj");
		monkey_normal = load_texture_map("data/dragon_scene/textures/suzanne_texture_normal.png");
		monkey_color = load_texture("data/dragon_scene/textures/suzanne_texture_color.png");

		ph::Material monkey_mat{ &monkey_color, nullptr, &monkey_normal };
		monkey_material = add_material(monkey_mat);

		// Load skybox
		std::array<std::string_view, 6> faces = {
			{
				// +X, -X, +Y, -Y, +Z, -Z
				"data/dragon_scene/textures/cubemap_r.png",
				"data/dragon_scene/textures/cubemap_l.png",
				"data/dragon_scene/textures/cubemap_u.png",
				"data/dragon_scene/textures/cubemap_d.png",
				"data/dragon_scene/textures/cubemap_b.png",
				"data/dragon_scene/textures/cubemap_f.png",
			}
		};
		skybox = load_cubemap(faces);
	}

	DragonScene() : ph::TestApplication(1280, 720, "Phobos Demo Scene (dragon)") {
		// Add render attachments
		present->add_color_attachment("color");
		present->add_depth_attachment("depth");

		load_resources();

		light.color = glm::vec3(1, 1, 1);
		light.position = glm::vec3(2, 2, 2);
		light.intensity = 1.0f;
	}

	void update(ph::FrameInfo& frame, ph::RenderGraph& render_graph) override {
		ph::RenderAttachment& color = present->get_attachment("color");
		ph::RenderAttachment& depth = present->get_attachment("depth");
		// Make ImGui UI
		if (ImGui::Begin("demo scene")) {
			ImVec2 const size = match_attachment_to_window_size(color);
			// Note that we can discard the return value here
			match_attachment_to_window_size(depth);
			ImGui::Image(color.get_imgui_tex_id(), size);
		}
		ImGui::End();

		if (ImGui::Begin("Frame stats")) {
			ImGui::TextUnformatted(
				fmt::format("Frametime: {} ms\nFPS: {}", (float)(frame_time * 1000.0f) , (int)(1.0f / frame_time)).c_str());
			ImGui::DragFloat3("dragon pos", &dragon_pos.x, 0.1f);
			ImGui::DragFloat3("monkey pos", &monkey_pos.x, 0.1f);
		}
		ImGui::End();

		monkey_rotation += 0.2f;

		// Add lights
		render_graph.point_lights.push_back(light);
		// Add materials
		render_graph.materials = materials;
		// Setup camera data
		render_graph.projection = projection(glm::radians(45.0f), 0.1f, 100.0f, color);
		render_graph.camera_pos = { 4.0f, 4.0f, 4.0f };
		render_graph.view = glm::lookAt(render_graph.camera_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

		// Setup info for main renderpass
		ph::RenderPass main_pass;
		main_pass.debug_name = "Main Pass";
		main_pass.outputs = { color, depth };
		main_pass.clear_values = { vk::ClearColorValue{std::array<float, 4>{ {0.427f, 0.427f, 0.427f, 1.0f}}}, clear_depth };

		main_pass.skybox = &skybox;

		// Dragon
		main_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &dragon, dragon_material });
		glm::mat4 dragon_transform = glm::translate(glm::mat4(1.0f), dragon_pos);
		dragon_transform = glm::scale(dragon_transform, glm::vec3(0.5, 0.5, 0.5));
		main_pass.transforms.push_back(dragon_transform);
		// Ground
		main_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &ground, ground_material });
		glm::mat4 ground_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
		ground_transform = glm::scale(ground_transform, glm::vec3(2.0f, 2.0f, 2.0f));
		main_pass.transforms.push_back(ground_transform);

		main_pass.draw_commands.push_back(ph::RenderPass::DrawCommand{ &monkey, monkey_material });
		glm::mat4 monkey_transform = glm::translate(glm::mat4(1.0f), monkey_pos);
		monkey_transform = glm::rotate(monkey_transform, glm::radians(monkey_rotation), glm::vec3(0, 1, 0));
		monkey_transform = glm::scale(monkey_transform, glm::vec3(0.5f, 0.5f, 0.5f));
		main_pass.transforms.push_back(monkey_transform);

		// Create callback which uses default draw commands. Note that this api is subject to change
		main_pass.callback = [&frame, this](ph::CommandBuffer& cmd_buf) {
			renderer->execute_draw_commands(frame, cmd_buf);
		};
		// Add pass to the rendergraph
		render_graph.add_pass(stl::move(main_pass));
	}
};

int main() {
	DragonScene app;
	app.run();
}