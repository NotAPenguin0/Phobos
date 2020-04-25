#ifndef PHOBOS_FIXED_DEFERRED_HPP_
#define PHOBOS_FIXED_DEFERRED_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/present/present_manager.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/fixed/skybox_renderer.hpp>
#include <phobos/fixed/lighting_pass.hpp>
#include <phobos/fixed/projection.hpp>

namespace ph::fixed {

class DeferredRenderer {
public:
	// Initializes the deferred pipeline. This function creates the following resources:
	// Render attachments:
	// You can retrieve these by querying for them in the present manager. The names of the attachments are
	// fixed_depth - depth buffer
	// fixed_normal - normal buffer
	// fixed_albedo_spec - albedo/specular color buffer
	// Pipelines:
	// Currently, the deferred renderer needs two pipelines to function. You can get them in the pipeline manager in the vulkan context.
	// They are stored with the following names:
	// fixed_deferred_main - pipeline for the main deferred rendering pass
	// fixed_deferred_lighting - pipeline for the deferred lighting pass
	DeferredRenderer(ph::VulkanContext& ctx, ph::PresentManager& present, vk::Extent2D resolution);
	
	// Must be called after a frame is rendered
	void frame_end();

	// Resizes attachments
	void resize(vk::Extent2D size);

	// Adds a new draw command to the deferred renderer.
	void add_draw(ph::Mesh& mesh, uint32_t material_index, glm::mat4 const& transform);
	// Adds a material to the internal material list and returns it index. This index is guaranteed to remain stable until you call frame_end().
	// Use the index to specify which material to use in add_draw().
	uint32_t add_material(ph::Material const& material);

	// Note: to enable the frustum test, you must call this after setting the camera data.
	void add_point_light(ph::PointLight const& light);

	// Sets the skybox to use for rendering. If this is nullptr, no skybox will be drawn.
	// Note that frame_end() resets this to nullptr.
	void set_skybox(ph::Cubemap* sb);

	// Builds all renderpasses for the deferred rendering pipeline.
	void build(ph::FrameInfo& frame, ph::RenderAttachment& output, ph::RenderGraph& graph, ph::Renderer& renderer);

	// Public resources.

	// This contains the camera data, make sure to set it every frame as it is reset by end_frame()
	CameraData camera_data;

private:

	// Resources

	struct Draw {
		ph::Mesh* mesh = nullptr;
		uint32_t material_index = 0;
		uint32_t transform_index = 0;
	};

	std::vector<Draw> draws;
	std::vector<glm::mat4> transforms;
	std::vector<ph::Material> materials;

	ph::RenderAttachment* depth;
	ph::RenderAttachment* normal;
	ph::RenderAttachment* albedo_spec;

	struct PerFrameResources {
		ph::BufferSlice transforms;
		ph::BufferSlice camera;
	} per_frame_resources;

	// Bindings

	struct MainPassBindings {
		ph::ShaderInfo::BindingInfo camera;
		ph::ShaderInfo::BindingInfo transforms;
		ph::ShaderInfo::BindingInfo textures;
	} bindings;


	// Rendering systems
	void build_main_pass(ph::FrameInfo& frame, ph::RenderGraph& graph, ph::Renderer& renderer);
	LightingPass lighting;
	SkyboxRenderer skybox;

	void create_main_pipeline(ph::VulkanContext& ctx);

	void update_transforms(ph::CommandBuffer& cmd_buf);
	void update_camera_data(ph::CommandBuffer& cmd_buf);
	vk::DescriptorSet get_main_pass_descriptors(ph::FrameInfo& frame, ph::Renderer& renderer, ph::RenderPass& pass);
};

}

#endif