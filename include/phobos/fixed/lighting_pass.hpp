#ifndef PHOBOS_FIXED_LIGHTING_PASS_HPP_
#define PHOBOS_FIXED_LIGHTING_PASS_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/renderer/command_buffer.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/renderer/light.hpp>
#include <phobos/fixed/projection.hpp>
namespace ph::fixed {

class LightingPass {
public:
	LightingPass(ph::VulkanContext& ctx);

	// Resets internal resources
	void frame_end();

	// Projection parameter will be used to calculate whether to actually add the light based on a frustum test.
	void add_point_light(Projection projection, ph::PointLight const& light);

	void build_render_pass(ph::FrameInfo& frame, ph::RenderAttachment& output, ph::RenderAttachment& depth, ph::RenderAttachment& normal, 
		ph::RenderAttachment& albedo_spec, ph::RenderGraph& graph, ph::Renderer& renderer, CameraData const& camera);
private:
	// Bindings

	struct LightingPassBindings {
		ph::ShaderInfo::BindingInfo depth;
		ph::ShaderInfo::BindingInfo normal;
		ph::ShaderInfo::BindingInfo albedo_spec;
		ph::ShaderInfo::BindingInfo lights;
		ph::ShaderInfo::BindingInfo camera;
	} bindings;

	// Resources

	std::vector<ph::PointLight> point_lights;
	ph::Mesh light_volume;

	struct PerFrameBuffers {
		ph::BufferSlice camera;
		ph::BufferSlice lights;
	} per_frame_buffers;

	void create_pipeline(ph::VulkanContext& ctx);
	void create_light_volume_mesh(ph::VulkanContext& ctx);

	void update_camera(ph::CommandBuffer& cmd_buf, CameraData const& camera);
	void update_lights(ph::CommandBuffer& cmd_buf);
};

}

#endif
