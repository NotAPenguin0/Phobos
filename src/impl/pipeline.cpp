// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Shader reflection
//		- Pipeline management

#include <phobos/context.hpp>
#include <phobos/impl/context.hpp>
#include <phobos/impl/pipeline.hpp>
#include <phobos/impl/cache.hpp>
#include <phobos/impl/buffer.hpp>

#include <phobos/pipeline.hpp>

#include <spirv_cross.hpp>
#include <algorithm>

namespace ph {
	namespace reflect {
	static ShaderStage get_shader_stage(spirv_cross::Compiler& refl) {
		auto entry_point_name = refl.get_entry_points_and_stages()[0];
		auto entry_point = refl.get_entry_point(entry_point_name.name, entry_point_name.execution_model);

		switch (entry_point.model) {
		case spv::ExecutionModel::ExecutionModelVertex: return ShaderStage::Vertex;
		case spv::ExecutionModel::ExecutionModelFragment: return ShaderStage::Fragment;
		case spv::ExecutionModel::ExecutionModelGLCompute: return ShaderStage::Compute;
		case spv::ExecutionModel::ExecutionModelRayGenerationKHR: return ShaderStage::RayGeneration;
		case spv::ExecutionModel::ExecutionModelClosestHitKHR: return ShaderStage::ClosestHit;
		case spv::ExecutionModel::ExecutionModelMissKHR: return ShaderStage::RayMiss;
		case spv::ExecutionModel::ExecutionModelAnyHitKHR: return ShaderStage::AnyHit;
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
		return refl;
	}

	// This function merges all push constant ranges in a single shader stage into one push constant range.
	// We need this because spirv-cross reports one push constant range for each variable, instead of for each block.
	static void merge_ranges(std::vector<VkPushConstantRange>& out, std::vector<VkPushConstantRange> const& in,
		ShaderStage stage) {

		VkPushConstantRange merged{};
		merged.size = 0;
		merged.offset = 1000000; // some arbitrarily large value to start with, we'll make this smaller using std::min() later
		merged.stageFlags = static_cast<VkShaderStageFlagBits>(stage);
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
					pc_range.stageFlags = static_cast<VkShaderStageFlagBits>(stage);
					pc_ranges.push_back(pc_range);
				}
			}
			merge_ranges(final, pc_ranges, stage);
		}
		return final;
	}

	static void find_uniform_buffers(ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = static_cast<VkShaderStageFlagBits>(get_shader_stage(refl));
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
		VkShaderStageFlags const stage = static_cast<VkShaderStageFlagBits>(get_shader_stage(refl));
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
		VkShaderStageFlags const stage = static_cast<VkShaderStageFlagBits>(get_shader_stage(refl));
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
		VkShaderStageFlags const stage = static_cast<VkShaderStageFlagBits>(get_shader_stage(refl));
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

#if PHOBOS_ENABLE_RAY_TRACING
	static void find_acceleration_structures(ShaderMeta& info, spirv_cross::Compiler& refl, DescriptorSetLayoutCreateInfo& dslci) {
		VkShaderStageFlags const stage = static_cast<VkShaderStageFlagBits>(get_shader_stage(refl));
		spirv_cross::ShaderResources res = refl.get_shader_resources();

		for (auto& as : res.acceleration_structures) {
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = refl.get_decoration(as.id, spv::DecorationBinding);
			binding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
			binding.stageFlags = stage;
			binding.descriptorCount = 1;
			info.add_binding(refl.get_name(as.id), { binding.binding, binding.descriptorType });
			dslci.bindings.push_back(binding);
		}
	}
#endif

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
#if PHOBOS_ENABLE_RAY_TRACING
			find_acceleration_structures(info, *refl, dslci);
#endif
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

PipelineImpl::PipelineImpl(ContextImpl& ctx, CacheImpl& cache, BufferImpl& buffer) 
	: ctx(&ctx), cache(&cache), buffer(&buffer) {
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

ShaderHandle PipelineImpl::create_shader(std::string_view path, std::string_view entry_point, ShaderStage stage) {
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

#if PHOBOS_ENABLE_RAY_TRACING

void PipelineImpl::reflect_shaders(ph::RayTracingPipelineCreateInfo& pci) {
	std::vector<std::unique_ptr<spirv_cross::Compiler>> reflected_shaders;
	for (auto handle : pci.shaders) {
		ph::ShaderModuleCreateInfo* shader = cache->shader.get(handle);
		assert(shader && "Invalid shader");
		reflected_shaders.push_back(reflect::reflect_shader_stage(*shader));
	}
	pci.layout = reflect::make_pipeline_layout(*ctx, reflected_shaders, pci.meta);
}

void PipelineImpl::create_named_pipeline(ph::RayTracingPipelineCreateInfo pci) {
	rtx_pipelines[pci.name] = std::move(pci);
}

ShaderMeta const& PipelineImpl::get_ray_tracing_shader_meta(std::string_view pipeline_name) {
	return get_ray_tracing_pipeline(pipeline_name).meta;
}

ph::RayTracingPipelineCreateInfo& PipelineImpl::get_ray_tracing_pipeline(std::string_view name) {
	return rtx_pipelines.at(std::string(name));
}

ShaderBindingTable PipelineImpl::create_shader_binding_table(std::string_view pipeline_name) {
	ph::RayTracingPipelineCreateInfo& pci = get_ray_tracing_pipeline(pipeline_name);
	// Make sure the pipeline is created before trying to build a SBT.
	ph::Pipeline pipeline = cache->get_or_create_ray_tracing_pipeline(pci);

	uint32_t const group_count = pci.shader_groups.size();
	uint32_t const group_handle_size = ctx->phys_device.ray_tracing_properties.shaderGroupHandleSize;
	uint32_t const group_alignment = ctx->phys_device.ray_tracing_properties.shaderGroupBaseAlignment;
	// Align the group size to the group alignmnet using the formula
	uint32_t const aligned_group_size = (group_handle_size + (group_alignment - 1)) & ~(group_alignment - 1);
	// Final size in bytes of the shader binding table
	uint32_t const sbt_byte_size = aligned_group_size * group_count;

	ph::RawBuffer sbt_buffer = buffer->create_buffer(ph::BufferType::ShaderBindingTable, sbt_byte_size);
	ctx->name_object(sbt_buffer.handle, "[RT SBT] " + std::string(pipeline_name));

	// Read back the shader handle data from the created pipeline
	std::vector<std::byte> shader_handle_data{ sbt_byte_size };

	// This function is only used here, and we don't have access to the main context, so we load it now
	static auto _vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetInstanceProcAddr(
		ctx->instance, "vkGetRayTracingShaderGroupHandlesKHR");
	_vkGetRayTracingShaderGroupHandlesKHR(ctx->device, pipeline.handle, 0, group_count,
		sbt_byte_size, shader_handle_data.data());

	// Write the handles to the SBT buffer
	std::byte* sbt_data = buffer->map_memory(sbt_buffer);
	std::byte* src_data = shader_handle_data.data();
	for (uint32_t group = 0; group < group_count; ++group) {
		std::memcpy(sbt_data, src_data, aligned_group_size);
		// Adjust pointers to next group
		sbt_data += aligned_group_size;
		src_data += group_handle_size;
	}

	buffer->unmap_memory(sbt_buffer);

	ShaderBindingTable sbt{};
	sbt.buffer = sbt_buffer;

	// Set group information in SBT. This is needed for when calling vkCmdTraceRaysKHR
	sbt.group_size_bytes = aligned_group_size;

	// Functor to more easily write the count_if and find() calls
	struct FindShaderGroupType {
		RayTracingShaderGroupType type{};

		bool operator()(RayTracingShaderGroup const& group) const {
			return group.type == type;
		}
	};

	sbt.raygen_count = std::count_if(pci.shader_groups.begin(), pci.shader_groups.end(), 
		FindShaderGroupType{RayTracingShaderGroupType::RayGeneration});
	sbt.raymiss_count = std::count_if(pci.shader_groups.begin(), pci.shader_groups.end(),
		FindShaderGroupType{ RayTracingShaderGroupType::RayMiss });
	sbt.rayhit_count = std::count_if(pci.shader_groups.begin(), pci.shader_groups.end(),
		FindShaderGroupType{ RayTracingShaderGroupType::RayHit });
	sbt.callable_count = std::count_if(pci.shader_groups.begin(), pci.shader_groups.end(),
		FindShaderGroupType{ RayTracingShaderGroupType::Callable });
	// Ray generation is always first, so this offset is always zero (cfr the std::sort() call).
	sbt.raygen_offset = 0;
	// Only set offset if there is a raymiss group, otherwise leaving it at zero is fine.
	if (sbt.raymiss_count > 0) {
		// The offset is equal to the index of the first raymiss group
		sbt.raymiss_offset = std::find_if(pci.shader_groups.begin(), pci.shader_groups.end(),
			FindShaderGroupType{ RayTracingShaderGroupType::RayMiss }) - pci.shader_groups.begin();
	}

	// Equivalent for rayhit and callable groups

	if (sbt.rayhit_count > 0) {
		sbt.rayhit_offset = std::find_if(pci.shader_groups.begin(), pci.shader_groups.end(),
			FindShaderGroupType{ RayTracingShaderGroupType::RayHit }) - pci.shader_groups.begin();
	}
	if (sbt.callable_count > 0) {
		// The offset is equal to the index of the first raymiss group
		sbt.callable_offset = std::find_if(pci.shader_groups.begin(), pci.shader_groups.end(),
			FindShaderGroupType{ RayTracingShaderGroupType::Callable }) - pci.shader_groups.begin();
	}

	VkDeviceAddress const sbt_address = buffer->get_device_address(sbt.buffer);
	uint32_t const group_stride = sbt.group_size_bytes;
	uint32_t const group_size = sbt.group_size_bytes;
	using Region = VkStridedDeviceAddressRegionKHR;
	sbt.regions = {
		Region { sbt_address, group_stride, group_size * sbt.raygen_count },
		Region { sbt_address + sbt.raymiss_offset * group_size, group_stride, group_size * sbt.raymiss_count},
		Region { sbt_address + sbt.rayhit_offset * group_size, group_stride, group_size * sbt.rayhit_count},
		Region { sbt.callable_count > 0 ? sbt_address + sbt.callable_count * group_size : 0,
				 sbt.callable_count > 0 ? group_stride : 0,
				 group_size * sbt.callable_count
			   }
	};
	sbt.address = sbt_address;

	return sbt;
}

#endif

}
}