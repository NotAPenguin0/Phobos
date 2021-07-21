#pragma once

#include <phobos/context.hpp>

namespace ph {
namespace impl {
	
class PipelineImpl {
public:
	PipelineImpl(ContextImpl& ctx, CacheImpl& cache);
	~PipelineImpl();

	// PUBLIC API

	ShaderHandle create_shader(std::string_view path, std::string_view entry_point, PipelineStage stage);
	void reflect_shaders(ph::PipelineCreateInfo& pci);
	void reflect_shaders(ph::ComputePipelineCreateInfo& pci);
	void create_named_pipeline(ph::PipelineCreateInfo pci);
	void create_named_pipeline(ph::ComputePipelineCreateInfo pci);
	ShaderMeta const& get_shader_meta(std::string_view pipeline_name);
	ShaderMeta const& get_compute_shader_meta(std::string_view pipeline_name);

	// PRIVATE API

	VkSampler basic_sampler = nullptr;
	ph::PipelineCreateInfo& get_pipeline(std::string_view name);
	ph::ComputePipelineCreateInfo& get_compute_pipeline(std::string_view name);

private:
	ContextImpl* ctx;
	CacheImpl* cache;

	std::unordered_map<std::string, ph::PipelineCreateInfo> pipelines{};
	std::unordered_map<std::string, ph::ComputePipelineCreateInfo> compute_pipelines;

};

}
}