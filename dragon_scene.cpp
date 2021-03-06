#include "test_app_framework.hpp"

#include <stl/literals.hpp>
#include <iostream>
#include <imgui/imgui.h>
#include <random>

#include <phobos/renderer/cubemap.hpp>
#include <phobos/fixed/fixed_pipeline.hpp>

class DragonScene : public ph::TestApplication {
private:
	ph::Texture spec_map;

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

	std::vector<ph::PointLight> lights;
	
	ph::Cubemap skybox;

	std::unique_ptr<ph::fixed::DeferredRenderer> deferred_renderer;
public:
	void load_resources() {
		spec_map = load_texture_map("data/dragon_scene/textures/blank.png");

		dragon = load_obj("data/dragon_scene/meshes/dragon.obj");
		// Make sure to call load_texture_map() for non-color textures or Srgb encoding will haunt you forever
		dragon_normal = load_texture_map("data/dragon_scene/textures/dragon_texture_normal.png");
		dragon_color = load_texture("data/dragon_scene/textures/dragon_texture_color.png");

		ph::Material dragon_mat{ &dragon_color, &spec_map, &dragon_normal };
		dragon_material = add_material(dragon_mat);

		ground = load_obj("data/dragon_scene/meshes/plane.obj");
		ground_normal = load_texture_map("data/dragon_scene/textures/plane_texture_normal.png");
		ground_color = load_texture("data/dragon_scene/textures/plane_texture_color.png");
		
		ph::Material ground_mat{ &ground_color, &spec_map, &ground_normal };
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

		std::random_device seed{};
		std::mt19937 rng{ seed() };
		std::uniform_real_distribution<float> pos_distr(-3.0f, 3.0f);
		std::uniform_real_distribution<float> radius_distr(0.1f, 1.0f);
		std::uniform_real_distribution<float> height_distr(0.0f, 3.0f);
		std::uniform_real_distribution<float> intensity_distr(0.5f, 1.0f);
		constexpr size_t light_cnt = 100;
		lights.reserve(light_cnt);
		for (size_t i = 0; i < light_cnt; ++i) {
			ph::PointLight light;
			light.color = glm::vec3(1, 1, 1);
			light.position.x = pos_distr(rng);
			light.position.y = height_distr(rng);
			light.position.z = pos_distr(rng);
			light.radius = radius_distr(rng);
			//light.intensity = intensity_distr(rng);
			lights.push_back(light);
		}

		deferred_renderer = std::make_unique<ph::fixed::DeferredRenderer>(*ctx, *present, vk::Extent2D{ 1280, 720 });
		
		present->add_color_attachment("scene", vk::Extent2D{ 1280, 720 }, vk::Format::eR8G8B8A8Srgb);
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
			ImGui::Image(ImGui_ImplPhobos_AddTexture(scene.image_view()), size);
			deferred_renderer->resize({ (uint32_t)size.x, (uint32_t)size.y });
		}
		ImGui::End();

		static bool light_overlay = false;

		if (ImGui::Begin("Frame stats")) {
			ImGui::TextUnformatted(
				fmt::format("Frametime: {} ms\nFPS: {}", (float)(frame_time * 1000.0f) , (int)(1.0f / frame_time)).c_str());
			ImGui::DragFloat3("dragon pos", &dragon_pos.x, 0.1f);
			ImGui::DragFloat3("monkey pos", &monkey_pos.x, 0.1f);
//			ImGui::ColorEdit3("light color", &light.color.x);
//			ImGui::DragFloat3("light pos", &light.position.x, 0.1f);
//			ImGui::DragFloat("light radius", &light.radius, 0.1f);
//			ImGui::DragFloat("light intensity", &light.intensity, 0.1f);
			ImGui::Checkbox("Light overlay", &light_overlay);
		}
		ImGui::End();

		monkey_rotation += 0.2f;

		deferred_renderer->camera_data.projection = { 0.1f, 100.0f, glm::radians(45.0f), 1280.0f / 720.0f };
		deferred_renderer->camera_data.position = { 4.0f, 4.0f, 4.0f };
		deferred_renderer->camera_data.view = glm::lookAt(deferred_renderer->camera_data.position, glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

		deferred_renderer->set_light_wireframe_overlay(light_overlay);

		// Note that we should add lights after setting the projection so we can get frustum culling
		for (auto const& light : lights) {
			deferred_renderer->add_point_light(light);
		}

		for (auto const& mat : materials) { deferred_renderer->add_material(mat); }

		deferred_renderer->set_skybox(&skybox);

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
		deferred_renderer->build(frame, scene, render_graph, *renderer);
	}
};

int main() {
	DragonScene app;
	app.run();
}