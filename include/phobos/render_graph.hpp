#pragma once

#include <phobos/context.hpp>
#include <phobos/pass.hpp>

namespace ph {

class RenderGraph {
public:
	void add_pass(Pass pass);
	void build(Context& ctx);
private:
	friend class RenderGraphExecutor;

	enum class BarrierType {
		Image,
		Buffer,
		Memory
	};

	struct Barrier {
		union {
			VkImageMemoryBarrier image;
			VkBufferMemoryBarrier buffer;
			VkMemoryBarrier memory;
		};
		BarrierType type;
		plib::bit_flag<PipelineStage> src_stage;
		plib::bit_flag<PipelineStage> dst_stage;
	};

	struct BuiltPass {
		VkRenderPass handle = nullptr;
		VkFramebuffer framebuf = nullptr;
		VkExtent2D render_area{};
		// These barriers will be executed AFTER the renderpass.
		std::vector<Barrier> barriers;
	};

	std::vector<Pass> passes;
	std::vector<BuiltPass> built_passes;

	std::vector<VkAttachmentDescription> get_attachment_descriptions(Context& ctx, Pass* pass);
	VkImageLayout get_initial_layout(Context& ctx, Pass* pass, ResourceUsage const& resource);
	VkImageLayout get_final_layout(Context& ctx, Pass* pass, ResourceUsage const& resource);

	struct AttachmentUsage {
		VkPipelineStageFlags stage{};
		VkAccessFlags access{};
		Pass* pass = nullptr;
	};


	std::pair<ResourceUsage, Pass*> find_previous_usage(Context& ctx, Pass* current_pass, Attachment* attachment);
	std::pair<ResourceUsage, Pass*> find_next_usage(Context& ctx, Pass* current_pass, Attachment* attachment);

	std::pair<ResourceUsage, Pass*> find_previous_usage(Pass* current_pass, BufferSlice const* buffer);
	std::pair<ResourceUsage, Pass*> find_next_usage(Pass* current_pass, BufferSlice const* buffer);

	AttachmentUsage get_attachment_usage(std::pair<ResourceUsage, Pass*> const& res_usage);

	void create_pass_barriers(Pass& pass, BuiltPass& result);
};

class RenderGraphExecutor {
public:
	// Assumes .begin() has already been called on cmd_buf. Will not call .end() on it.
	// Render graph must be built before calling this function.
	void execute(ph::CommandBuffer& cmd_buf, RenderGraph& graph);
private:

};

}