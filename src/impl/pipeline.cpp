// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Shader reflection
//		- Pipeline management

#include <phobos/context.hpp>
#include <phobos/impl/context.hpp>
#include <phobos/impl/pipeline.hpp>
#include <phobos/impl/cache.hpp>

#include <phobos/pipeline.hpp>

#include <spirv_cross.hpp>

namespace ph {
	namespace reflect {
	static PipelineStage get_shader_stage(spirv_cross::Compiler& refl) {
		auto entry_point_name = refl.get_entry_points_and_stages()[0];
		auto entry_point = refl.get_entry_point(entry_point_name.name, entry_point_name.execution_model);

		switch (entry_point.model) {
		case spv::ExecutionModel::ExecutionModelVertex: return PipelineStage::VertexShader;
		case spv::ExecutionModel::ExecutionModelFragment: return PipelineStage::FragmentShader;
		case spv::ExecutionModel::ExecutionModelGLCompute: return PipelineStage::ComputeShader;
		default: return {};
		}
	}

	static VkFormat get_vk_format(uint32_t vecsize) {
		switch (vecsize) {
		case 1: return VK_FORMAT_R32_SFLOAT;
		case 2: return VK_FORMAT_R32G32_SFLOAT;
		case 3: return VK_FORMAT_R32G32B32_SFLOAT;
		case 4: return VK_FORMAT_R32G32B32A32_SFLOAT;
		default: return VK_FORMAT_UNDEFINED;
		}
	}

	static std::unique_ptr<spirv_cross::Compiler> reflect_shader_stage(ph::ShaderModuleCreateInfo& shader) {
		auto refl = std::make_unique<spirv_cross::Compiler>(shader.code);

		assert(refl && "Failed to reflect shader");
		spirv_cross::ShaderResources res = refl->get_shader_resources();

		return refl;
	}

	static VkShaderStageFlags pipeline_to_shader_stage(PipelineStage stage) {
		switch (stage) {
		case PipelineStage::VertexShader: return VK_SHADER_STAGE_VERTEX_BIT;
		case PipelineStage::FragmentShader: return VK_SHADER_STAGE_FRAGMENT_BIT;
		case PipelineStage::ComputeShader: return VK_SHADER_STAGE_COMPUTE_BIT;
		default: return {};
		}
	}

	// This function merges all push constant ranges in a single shader stage into one push constant range.
	// We need this because spirv-cross reports one push constant range for each variable, instead of for each block.
	static void merge_ranges(std::vector<VkPushConstantRange>& out, std::vector<VkPushConstantRange> const& in,
		PipelineStage stage) {

		VkPushConstantRange merged{};
		merged.size = 0;
		merged.offset = 1000000; // some arbitrarily large value to start with, we'll make this smaller using std::min() later
		merged.stageFlags = pipeline_to_shader_stage(stage);
		for (auto& range : in) {
			if (range.stageFlags == merged.stageFlags) {
				merged.offset = std::min(merged.offset, range.offset);
				merged.size += range.size;
			}
		}

		if (merged.size != 0) {
			out.push_back(merged);
		}
	}

	static std::vector<VkPushConstantRange> get_push_constants(std::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders) {
		std::vector<VkPushConstantRange> pc_ranges;
		std::vector<VkPushConstantRange> final;
		for (auto& refl : reflected_shaders) {
			spirv_cross::ShaderResources res = refl->get_shader_resources();
			auto const stage = get_shader_stage(*refl);
			for (auto& pc : res.push_constant_buffers) {
				auto ranges = refl->get_active_buffer_ranges(pc.id);
				for (auto& range : ranges) {
					VkPushConstantRange pc_range{};
					pc_range.offset = range.offset;
					pc_range.size = range.range;
					pc_range.stageFlags = pipeline_to_shader_stage(stage);
					pc_ranges.push_back(pc_range);
				}
			}
			merge_ranges(final, pc_ranges, stage);
		}
		return final;
	}

	static void find_uniform_buffers(ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = pipeline_to_shader_stage(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();
		for (auto& ubo : res.uniform_buffers) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(ubo.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			binding.descriptorCount = 1;
			binding.stageFlags = stage;
			dslci.bindings.push_back(binding);

			info.add_binding(refl.get_name(ubo.id), { binding.binding, binding.descriptorType });
		}
	}

	static void find_shader_storage_buffers(ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = pipeline_to_shader_stage(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();
		for (auto& ssbo : res.storage_buffers) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(ssbo.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			binding.descriptorCount = 1;
			binding.stageFlags = stage;
			dslci.bindings.push_back(binding);

			info.add_binding(refl.get_name(ssbo.id), { binding.binding, binding.descriptorType });
		}
	}

	static void find_sampled_images(impl::ContextImpl& ctx, ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = pipeline_to_shader_stage(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();
		for (auto& si : res.sampled_images) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(si.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			binding.stageFlags = stage;
			auto type = refl.get_type(si.type_id);
			// type.array has the dimensions of the array. If this is zero, we don't have an array.
			// If it's larger than zero, we have an array.
			if (type.array.size() > 0) {
				// Now the dimensions of the array are in the first value of the array field.
				// 0 means unbounded
				if (type.array[0] == 0) {
					binding.descriptorCount = ctx.max_unbounded_array_size;
					// An unbounded array of samplers means descriptor indexing, we have to set the PartiallyBound and VariableDescriptorCount
					// flags for this binding

					// Reserve enough space to hold all flags and this one
					dslci.flags.resize(dslci.bindings.size() + 1);
					dslci.flags.back() = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
				}
				else {
					binding.descriptorCount = type.array[0];
				}
			}
			else {
				// If it' not an array, there is only one descriptor
				binding.descriptorCount = 1;
			}

			info.add_binding(refl.get_name(si.id), { binding.binding, binding.descriptorType });
			dslci.bindings.push_back(binding);
		}
	}

	static void find_storage_images(ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = pipeline_to_shader_stage(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();

		for (auto& si : res.storage_images) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(si.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			binding.stageFlags = stage;
			binding.descriptorCount = 1;
			info.add_binding(refl.get_name(si.id), { binding.binding, binding.descriptorType });
			dslci.bindings.push_back(binding);
		}
	}

	static void collapse_bindings(DescriptorSetLayoutCreateInfo& info) {
		std::vector<VkDescriptorSetLayoutBinding> final_bindings;
		std::vector<VkDescriptorBindingFlags> final_flags;

		for (size_t i = 0; i < info.bindings.size(); ++i) {
			auto binding = info.bindings[i];

			// Before doing anything, check if we already processed this binding
			bool process = true;
			for (auto const& b : final_bindings) {
				if (binding.binding == b.binding) {
					process = false;
					break;
				}
			}
			if (!process) { continue; }

			VkShaderStageFlags stages = binding.stageFlags;
			for (size_t j = i + 1; j < info.bindings.size(); ++j) {
				auto const& other_binding = info.bindings[j];
				if (binding.binding == other_binding.binding) {
					stages |= other_binding.stageFlags;

				}
			}
			binding.stageFlags = stages;
			final_bindings.push_back(binding);
			if (!info.flags.empty()) {
				final_flags.push_back(info.flags[i]);
			}
		}

		info.bindings = final_bindings;
		info.flags = final_flags;
	}

	static DescriptorSetLayoutCreateInfo get_descriptor_set_layout(impl::ContextImpl& ctx, ShaderMeta& info, std::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders) {
		DescriptorSetLayoutCreateInfo dslci;
		for (auto& refl : reflected_shaders) {
			find_uniform_buffers(info, *refl, dslci);
			find_shader_storage_buffers(info, *refl, dslci);
			find_sampled_images(ctx, info, *refl, dslci);
			find_storage_images(info, *refl, dslci);
		}
		collapse_bindings(dslci);
		return dslci;
	}

	static PipelineLayoutCreateInfo make_pipeline_layout(impl::ContextImpl& ctx, std::vector<std::unique_ptr<spirv_cross::Compiler>>& reflected_shaders, ShaderMeta& shader_info) {
		PipelineLayoutCreateInfo layout;
		layout.push_constants = get_push_constants(reflected_shaders);
		layout.set_layout = get_descriptor_set_layout(ctx, shader_info, reflected_shaders);
		return layout;
	}
}

namespace impl {

PipelineImpl::PipelineImpl(ContextImpl& ctx, CacheImpl& cache) : ctx(&ctx), cache(&cache) {
	VkSamplerCreateInfo sci{
		.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.magFilter = VK_FILTER_LINEAR,
		.minFilter = VK_FILTER_LINEAR,
		.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias = 0.0f,
		.anisotropyEnable = false,
		.maxAnisotropy = 0.0f,
		.compareEnable = false,
		.compareOp = VK_COMPARE_OP_ALWAYS,
		.minLod = 0.0f,
		.maxLod = 64.0f,
		.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
		.unnormalizedCoordinates = false
	};
	vkCreateSampler(ctx.device, &sci, nullptr, &basic_sampler);
}

PipelineImpl::~PipelineImpl() {
	vkDestroySampler(ctx->device, basic_sampler, nullptr);
}


namespace {

	struct ShaderHandleId {
		static inline std::atomic<uint32_t> cur = 0;
		static inline uint32_t next() {
			return cur++;
		}
	};

}

ShaderHandle PipelineImpl::create_shader(std::string_view path, std::string_view entry_point, PipelineStage stage) {
	uint32_t id = ShaderHandleId::next();

	ph::ShaderModuleCreateInfo info;
	info.code = load_shader_code(path);
	info.entry_point = entry_point;
	info.stage = stage;

	cache->shader.insert(ShaderHandle{ id }, std::move(info));

	return ShaderHandle{ id };
}


void PipelineImpl::create_named_pipeline(ph::PipelineCreateInfo pci) {
	pipelines[pci.name] = std::move(pci);
}

void PipelineImpl::create_named_pipeline(ph::ComputePipelineCreateInfo pci) {
	compute_pipelines[pci.name] = std::move(pci);
}

ShaderMeta const& PipelineImpl::get_shader_meta(std::string_view pipeline_name) {
	return get_pipeline(pipeline_name).meta;
}

ShaderMeta const& PipelineImpl::get_compute_shader_meta(std::string_view pipeline_name) {
	return get_compute_pipeline(pipeline_name).meta;
}


void PipelineImpl::reflect_shaders(ph::PipelineCreateInfo& pci) {
	std::vector<std::unique_ptr<spirv_cross::Compiler>> reflected_shaders;
	for (auto handle : pci.shaders) {
		ph::ShaderModuleCreateInfo* shader = cache->shader.get(handle);
		assert(shader && "Invalid shader");
		reflected_shaders.push_back(reflect::reflect_shader_stage(*shader));
	}
	pci.layout = reflect::make_pipeline_layout(*ctx, reflected_shaders, pci.meta);
}

void PipelineImpl::reflect_shaders(ph::ComputePipelineCreateInfo& pci) {
	std::vector<std::unique_ptr<spirv_cross::Compiler>> reflected_shaders;
	ph::ShaderModuleCreateInfo* shader = cache->shader.get(pci.shader);
	assert(shader && "Invalid shader");
	reflected_shaders.push_back(reflect::reflect_shader_stage(*shader));
	pci.layout = reflect::make_pipeline_layout(*ctx, reflected_shaders, pci.meta);
}


ph::PipelineCreateInfo& PipelineImpl::get_pipeline(std::string_view name) {
	return pipelines.at(std::string(name));
}

ph::ComputePipelineCreateInfo& PipelineImpl::get_compute_pipeline(std::string_view name) {
	return compute_pipelines.at(std::string(name));
}

}
}