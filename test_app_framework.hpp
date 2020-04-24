#ifndef PHOBOS_TEST_APP_FRAMEWORK_HPP_
#define PHOBOS_TEST_APP_FRAMEWORK_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/core/window_context.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/renderer/render_graph.hpp>
#include <phobos/renderer/cubemap.hpp>
#include <phobos/present/present_manager.hpp>
#include <phobos/pipeline/pipeline.hpp>

#include <phobos/util/shader_util.hpp>

#include <stl/unique_ptr.hpp>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui/imgui.h>

#undef near
#undef far

namespace ph {

class DefaultLogger : public ph::log::LogInterface {
public:
	void write(ph::log::Severity severity, std::string_view str) override;
};

class TestApplication {
public:
	TestApplication(size_t width, size_t height, const char* title);
	~TestApplication();

	void run();
protected:
	virtual void update(FrameInfo& frame, RenderGraph& render_graph) = 0;
	virtual void frame_end() {}

	Mesh generate_cube_geometry();
	Mesh generate_quad_geometry();
	Mesh generate_plane_geometry();
	Texture load_texture(std::string_view path);
	// Loads as non-srgb
	Texture load_texture_map(std::string_view path);
	// Faces must be specified in order +X, -X, +Y, -Y, +Z, -Z
	// = Right, Left, Up, Down, Back, Front
	Cubemap load_cubemap(std::array<std::string_view, 6> faces);

	// Returns the new size of the attachment
	ImVec2 match_attachment_to_window_size(RenderAttachment& attachment);

	// Get projection matrix for camera matching dimensions of this attachment
	glm::mat4 projection(float fov, float near, float far, RenderAttachment const& attachment);
	glm::mat4 projection(float fov, float near, float far, float aspect);

	Mesh load_obj(std::string_view path);
	uint32_t add_material(Material const& mat);

	stl::vector<Material> materials;

	double time;
	double frame_time;

	size_t width, height;
	VulkanContext* ctx;
	WindowContext* window;
	DefaultLogger logger;

	stl::unique_ptr<PresentManager> present;
	stl::unique_ptr<Renderer> renderer;

	// Default clearvalues for color and depth
	vk::ClearColorValue clear_color = vk::ClearColorValue{ std::array<float, 4>{ {0.0f, 0.0f, 0.0f, 1.0f}} };
	vk::ClearDepthStencilValue clear_depth = vk::ClearDepthStencilValue{ 1.0f, 0 };
};

}

#endif