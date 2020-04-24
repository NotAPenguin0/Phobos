#include "test_app_framework.hpp"

#include <stl/literals.hpp>
#include <iostream>
#include <imgui/imgui.h>

#include <phobos/renderer/cubemap.hpp>
#include <phobos/fixed/fixed_pipeline.hpp>

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

	std::unique_ptr<ph::fixed::DeferredRenderer> deferred_renderer;
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
		load_resources();

		light.color = glm::vec3(1, 1, 1);
		light.position = glm::vec3(2, 2, 2);
		light.intensity = 1.0f;

		deferred_renderer = std::make_unique<ph::fixed::DeferredRenderer>(*ctx, *present, vk::Extent2D{ 1280, 720 });

		present->add_color_attachment("scene");
	}

	void frame_end() override {
		deferred_renderer->frame_end();
	}

	void update(ph::FrameInfo& frame, ph::RenderGraph& render_graph) override {
		ph::RenderAttachment& scene = present->get_attachment("scene");
		// Make ImGui UI
		if (ImGui::Begin("demo scene")) {
			ImVec2 const size = match_attachment_to_window_size(scene);
			// Note that we can discard the return value here
			ImGui::Image(scene.get_imgui_tex_id(), size);
			deferred_renderer->resize({ (uint32_t)size.x, (uint32_t)size.y });
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
		// Setup camera data
		render_graph.projection = projection(glm::radians(45.0f), 0.1f, 100.0f, 1280.0f/720.0f);
		render_graph.camera_pos = { 4.0f, 4.0f, 4.0f };
		render_graph.view = glm::lookAt(render_graph.camera_pos, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

		for (auto const& mat : materials) { deferred_renderer->add_material(mat); }

		glm::mat4 dragon_transform = glm::translate(glm::mat4(1.0f), dragon_pos);
		dragon_transform = glm::scale(dragon_transform, glm::vec3(0.5, 0.5, 0.5));
		deferred_renderer->add_draw(dragon, dragon_material, dragon_transform);
		
		glm::mat4 ground_transform = glm::translate(glm::mat4(1.0f), glm::vec3(0, 0, 0));
		ground_transform = glm::scale(ground_transform, glm::vec3(2.0f, 2.0f, 2.0f));
		deferred_renderer->add_draw(ground, ground_material, ground_transform);

		glm::mat4 monkey_transform = glm::translate(glm::mat4(1.0f), monkey_pos);
		monkey_transform = glm::rotate(monkey_transform, glm::radians(monkey_rotation), glm::vec3(0, 1, 0));
		monkey_transform = glm::scale(monkey_transform, glm::vec3(0.5f, 0.5f, 0.5f));
		deferred_renderer->add_draw(monkey, monkey_material, monkey_transform);
		deferred_renderer->build_main_pass(frame, render_graph, *renderer);
		deferred_renderer->build_resolve_pass(frame, scene, render_graph, *renderer);
	}
};

int main() {
	DragonScene app;
	app.run();
}