#pragma once

#include <string_view>
#include <vector>
#include <functional>
#include <phobos/command_buffer.hpp>
#include <vulkan/vulkan.h>

namespace ph {

struct ClearColor {
	float color[4]{ 0.0f, 0.0f, 0.0f, 0.1f };
};

struct ClearDepthStencil {
	float depth = 0.0f;
	uint32_t stencil = 0;
};

union ClearValue {
	ClearColor color;
	ClearDepthStencil depth_stencil;
};

enum class LoadOp {
	DontCare = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	Load = VK_ATTACHMENT_LOAD_OP_LOAD,
	Clear = VK_ATTACHMENT_LOAD_OP_CLEAR
};

struct PassOutput {
	std::string name = "";
	LoadOp load_op = LoadOp::DontCare;
	ClearValue clear{ .color{} };
};

struct Pass {
	// List of names of the attachments sampled by this pass.
	// TODO: Replace with proper resource-usage list so we can put all resources in here
	std::vector<std::string_view> sampled_attachments{};
	// List of attachments this pass writes to.
	// TODO: Replace with proper resource-usage list
	std::vector<PassOutput> outputs{};
	// Name of this pass
	std::string name = "";
	// Execution callback
	std::function<void(ph::CommandBuffer&)> execute{};
};

}