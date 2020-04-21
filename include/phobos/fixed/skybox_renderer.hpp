#ifndef PHOBOS_FIXED_SKYBOX_RENDERER_HPP_
#define PHOBOS_FIXED_SKYBOX_RENDERER_HPP_

#include <phobos/core/vulkan_context.hpp>
#include <phobos/renderer/command_buffer.hpp>
#include <phobos/renderer/renderer.hpp>
#include <phobos/renderer/cubemap.hpp>

namespace ph::fixed {

class SkyboxRenderer {
public:
	SkyboxRenderer(VulkanContext* ctx, Renderer* renderer) : ctx(ctx), renderer(renderer) {}

	void set_skybox(ph::Cubemap* sb) { skybox = sb; }
	void execute(ph::FrameInfo& frame, ph::CommandBuffer& cmd_buf);
private:
	VulkanContext* ctx = nullptr;
	Renderer* renderer = nullptr;

	ph::Cubemap* skybox = nullptr;
};

}

#endif