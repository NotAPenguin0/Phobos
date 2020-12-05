#pragma once

#include <string_view>
#include <vector>
#include <functional>

#include <vulkan/vulkan.h>

#include <plib/bit_flag.hpp>

#include <phobos/command_buffer.hpp>
#include <phobos/image.hpp>
#include <phobos/pipeline.hpp>

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


enum class ResourceAccess {
	ShaderRead = VK_ACCESS_SHADER_READ_BIT,
	ShaderWrite = VK_ACCESS_SHADER_WRITE_BIT,
	AttachmentOutput
};

enum class ResourceType {
	Image,
	Buffer,
	Attachment
};

struct ResourceUsage {
	plib::bit_flag<PipelineStage> stage{};
	ResourceAccess access{};

	struct Attachment {
		std::string name = "";
		LoadOp load_op = LoadOp::DontCare;
		ClearValue clear{ .color{} };
	};

	struct Image {
		ImageView view;
	};

	struct Buffer {

	};

	Attachment attachment{};
	Image image{};
	Buffer buffer{};
	ResourceType type{};
};

struct Pass {
	std::vector<ResourceUsage> resources{};
	// Name of this pass
	std::string name = "";
	// Execution callback
	std::function<void(ph::CommandBuffer&)> execute{};
};

class PassBuilder {
public:
	static PassBuilder create(std::string_view name);

	PassBuilder& add_attachment(std::string_view name, LoadOp load_op, ClearValue clear = { .color {} });
	PassBuilder& sample_attachment(std::string_view name, plib::bit_flag<PipelineStage> stage);
	PassBuilder& execute(std::function<void(ph::CommandBuffer&)> callback);

	Pass get();

private:
	Pass pass;
};

}