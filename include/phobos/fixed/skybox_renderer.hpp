#ifndef PHOBOS_FIXED_SKYBOX_RENDERER_HPP_
#define PHOBOS_FIXED_SKYBOX_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/renderer/command_buffer.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/renderer/cubemap.hpp>

namespace ph::fixed {

class SkyboxRenderer {
public:
	// Creates pipeline for the skybox renderer
	SkyboxRenderer(VulkanContext& ctx);

	void set_skybox(ph::Cubemap* sb) { skybox = sb; }
	// Note that this does not build a renderpass if no skybox is set
	void build_render_pass(ph::FrameInfo& frame, ph::RenderAttachment& output, ph::RenderAttachment& depth, ph::RenderGraph& graph, ph::Renderer& renderer);

private:
	// Resources

	ph::Cubemap* skybox = nullptr;

	// Bindings

	struct Bindings {
		ph::ShaderInfo::BindingInfo transform;
		ph::ShaderInfo::BindingInfo skybox;
	} bindings;

	void create_pipeline(VulkanContext& ctx);
};

}

#endif
