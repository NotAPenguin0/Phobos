#pragma once

#include <phobos/context.hpp>

namespace ph {
namespace impl {

class CacheImpl {
public:
	CacheImpl(Context& ctx, AppSettings const& settings);
	~CacheImpl();

	VkFramebuffer get_or_create_framebuffer(VkFramebufferCreateInfo const& info, std::string const& name = "");
	VkRenderPass get_or_create_renderpass(VkRenderPassCreateInfo const& info, std::string const& name = "");
	VkDescriptorSetLayout get_or_create_descriptor_set_layout(DescriptorSetLayoutCreateInfo const& dslci);
	PipelineLayout get_or_create_pipeline_layout(PipelineLayoutCreateInfo const& plci, VkDescriptorSetLayout set_layout);
	Pipeline get_or_create_pipeline(ph::PipelineCreateInfo& pci, VkRenderPass render_pass);
	Pipeline get_or_create_compute_pipeline(ph::ComputePipelineCreateInfo& pci);
#if PHOBOS_ENABLE_RAY_TRACING
	Pipeline get_or_create_ray_tracing_pipeline(ph::RayTracingPipelineCreateInfo& pci);
#endif
	VkDescriptorSet get_or_create_descriptor_set(DescriptorSetBinding set_binding, Pipeline const& pipeline, void* pNext = nullptr);

	void next_frame();

	Cache<VkFramebufferCreateInfo, VkFramebuffer> framebuffer;
	Cache<VkRenderPassCreateInfo, VkRenderPass> renderpass;
	Cache<ph::DescriptorSetLayoutCreateInfo, VkDescriptorSetLayout> set_layout;
	Cache<ph::PipelineLayoutCreateInfo, ph::PipelineLayout> pipeline_layout;
	Cache<ph::PipelineCreateInfo, ph::Pipeline> pipeline;
	Cache<ph::ComputePipelineCreateInfo, ph::Pipeline> compute_pipeline;
#if PHOBOS_ENABLE_RAY_TRACING
	Cache<ph::RayTracingPipelineCreateInfo, ph::Pipeline> rtx_pipeline{};
#endif
	Cache<ShaderHandle, ph::ShaderModuleCreateInfo> shader;
	RingBuffer<Cache<DescriptorSetBinding, VkDescriptorSet>> descriptor_set{};

	// TODO: automatically growing descriptor pool
	static constexpr size_t sets_per_type = 1024;
	VkDescriptorPool descr_pool = nullptr;
private:

	Context* ctx;
};

}
}