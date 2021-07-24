// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Cache management

#include <phobos/impl/cache.hpp>
#include <phobos/impl/context.hpp>

#include <cassert>

namespace ph {
namespace impl {

CacheImpl::CacheImpl(Context& ctx, AppSettings const& settings) : ctx(&ctx) {
	VkDescriptorPoolCreateInfo dpci{};
	dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	dpci.flags = {};
	dpci.pNext = nullptr;
	VkDescriptorPoolSize pool_sizes[]{
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets_per_type },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, sets_per_type },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, sets_per_type },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, sets_per_type },
#if PHOBOS_ENABLE_RAY_TRACING
		VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, sets_per_type },
#endif
	};
	dpci.maxSets = sets_per_type * sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize);
	dpci.poolSizeCount = sizeof(pool_sizes) / sizeof(VkDescriptorPoolSize);
	dpci.pPoolSizes = pool_sizes;
	vkCreateDescriptorPool(ctx.device(), &dpci, nullptr, &descr_pool);

	descriptor_set = RingBuffer<Cache<DescriptorSetBinding, VkDescriptorSet>>{ settings.max_frames_in_flight };
}

CacheImpl::~CacheImpl() {
	// Clear out all caches
	framebuffer.foreach([this](VkFramebuffer framebuf) {
		vkDestroyFramebuffer(ctx->device(), framebuf, nullptr);
	});
	renderpass.foreach([this](VkRenderPass pass) {
		vkDestroyRenderPass(ctx->device(), pass, nullptr);
	});
	set_layout.foreach([this](VkDescriptorSetLayout set_layout) {
		vkDestroyDescriptorSetLayout(ctx->device(), set_layout, nullptr);
	});
	pipeline_layout.foreach([this](ph::PipelineLayout ppl_layout) {
		vkDestroyPipelineLayout(ctx->device(), ppl_layout.handle, nullptr);
	});
	pipeline.foreach([this](ph::Pipeline pipeline) {
		vkDestroyPipeline(ctx->device(), pipeline.handle, nullptr);
	});
	compute_pipeline.foreach([this](ph::Pipeline pipeline) {
		vkDestroyPipeline(ctx->device(), pipeline.handle, nullptr);
	});
#if PHOBOS_ENABLE_RAY_TRACING
	rtx_pipeline.foreach([this](ph::Pipeline pipeline) {
		vkDestroyPipeline(ctx->device(), pipeline.handle, nullptr);
	});
#endif
	vkDestroyDescriptorPool(ctx->device(), descr_pool, nullptr);
}

VkFramebuffer CacheImpl::get_or_create_framebuffer(VkFramebufferCreateInfo const& info, std::string const& name) {
	{
		VkFramebuffer* framebuf = framebuffer.get(info);
		if (framebuf) { return *framebuf; }
	}

	VkFramebuffer framebuf = nullptr;
	vkCreateFramebuffer(ctx->device(), &info, nullptr, &framebuf);
	framebuffer.insert(info, framebuf);
	ctx->name_object(framebuf, name);
	return framebuf;
}

VkRenderPass CacheImpl::get_or_create_renderpass(VkRenderPassCreateInfo const& info, std::string const& name) {
	{
		VkRenderPass* pass = renderpass.get(info);
		if (pass) { return *pass; }
	}

	VkRenderPass pass = nullptr;
	vkCreateRenderPass(ctx->device(), &info, nullptr, &pass);
	renderpass.insert(info, pass);
	ctx->name_object(pass, name);
	return pass;
}

VkDescriptorSetLayout CacheImpl::get_or_create_descriptor_set_layout(DescriptorSetLayoutCreateInfo const& dslci) {
	auto set_layout_opt = set_layout.get(dslci);
	if (!set_layout_opt) {
		// We have to create the descriptor set layout here
		VkDescriptorSetLayoutCreateInfo set_layout_info{ };
		set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		set_layout_info.bindingCount = dslci.bindings.size();
		set_layout_info.pBindings = dslci.bindings.data();
		VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info{};
		if (!dslci.flags.empty()) {
			flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			assert(dslci.bindings.size() == dslci.flags.size() && "flag count doesn't match binding count");
			flags_info.bindingCount = dslci.bindings.size();
			flags_info.pBindingFlags = dslci.flags.data();
			set_layout_info.pNext = &flags_info;
		}
		VkDescriptorSetLayout set_layout = nullptr;
		vkCreateDescriptorSetLayout(ctx->device(), &set_layout_info, nullptr, &set_layout);
		// Store for further use when creating the pipeline layout
		this->set_layout.insert(dslci, set_layout);
		return set_layout;
	}
	else {
		return *set_layout_opt;
	}
}

PipelineLayout CacheImpl::get_or_create_pipeline_layout(PipelineLayoutCreateInfo const& plci, VkDescriptorSetLayout set_layout) {
	// Create or get pipeline layout from cache
	auto pipeline_layout_opt = pipeline_layout.get(plci);
	if (!pipeline_layout_opt) {
		// We have to create a new pipeline layout
		VkPipelineLayoutCreateInfo layout_create_info{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.setLayoutCount = 1,
			.pSetLayouts = &set_layout,
			.pushConstantRangeCount = (uint32_t)plci.push_constants.size(),
			.pPushConstantRanges = plci.push_constants.data(),
		};
		VkPipelineLayout vk_layout = nullptr;
		vkCreatePipelineLayout(ctx->device(), &layout_create_info, nullptr, &vk_layout);
		PipelineLayout layout;
		layout.handle = vk_layout;
		layout.set_layout = set_layout;
		this->pipeline_layout.insert(plci, layout);
		return layout;
	}
	else {
		return *pipeline_layout_opt;
	}
}


static VkShaderModule create_shader_module(VkDevice device, ph::ShaderModuleCreateInfo const& info) {
	VkShaderModuleCreateInfo vk_info{};
	vk_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	constexpr size_t bytes_per_spv_element = 4;
	vk_info.codeSize = info.code.size() * bytes_per_spv_element;
	vk_info.pCode = info.code.data();
	VkShaderModule module = nullptr;
	vkCreateShaderModule(device, &vk_info, nullptr, &module);
	return module;
}

Pipeline CacheImpl::get_or_create_pipeline(ph::PipelineCreateInfo& pci, VkRenderPass render_pass) {
	{
		Pipeline* pipeline = this->pipeline.get(pci);
		if (pipeline) { return *pipeline; }
	}

	// Set up pipeline create info
	VkGraphicsPipelineCreateInfo gpci{};
	VkDescriptorSetLayout set_layout = get_or_create_descriptor_set_layout(pci.layout.set_layout);
	ph::PipelineLayout layout = get_or_create_pipeline_layout(pci.layout, set_layout);
	gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpci.layout = layout.handle;
	gpci.renderPass = render_pass;
	gpci.subpass = 0;
	VkPipelineColorBlendStateCreateInfo blend_info{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.logicOpEnable = pci.blend_logic_op_enable,
		.logicOp = {},
		.attachmentCount = (uint32_t)pci.blend_attachments.size(),
		.pAttachments = pci.blend_attachments.data(),
		.blendConstants = {}
	};
	gpci.pColorBlendState = &blend_info;
	gpci.pDepthStencilState = &pci.depth_stencil;
	VkPipelineDynamicStateCreateInfo dynamic_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.dynamicStateCount = (uint32_t)pci.dynamic_states.size(),
		.pDynamicStates = pci.dynamic_states.data()
	};
	gpci.pDynamicState = &dynamic_state;
	gpci.pInputAssemblyState = &pci.input_assembly;
	gpci.pMultisampleState = &pci.multisample;
	gpci.pNext = nullptr;
	gpci.pRasterizationState = &pci.rasterizer;
	VkPipelineVertexInputStateCreateInfo vertex_input{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.vertexBindingDescriptionCount = (uint32_t)pci.vertex_input_bindings.size(),
		.pVertexBindingDescriptions = pci.vertex_input_bindings.data(),
		.vertexAttributeDescriptionCount = (uint32_t)pci.vertex_attributes.size(),
		.pVertexAttributeDescriptions = pci.vertex_attributes.data()
	};
	gpci.pVertexInputState = &vertex_input;
	VkPipelineViewportStateCreateInfo viewport_state{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.viewportCount = (uint32_t)pci.viewports.size(),
		.pViewports = pci.viewports.data(),
		.scissorCount = (uint32_t)pci.scissors.size(),
		.pScissors = pci.scissors.data()
	};
	gpci.pViewportState = &viewport_state;

	// Shader modules
	std::vector<VkPipelineShaderStageCreateInfo> shader_infos;
	for (auto const& handle : pci.shaders) {
		auto shader_info = this->shader.get(handle);
		assert(shader_info && "Invalid shader handle");
		VkPipelineShaderStageCreateInfo ssci{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.stage = static_cast<VkShaderStageFlagBits>(shader_info->stage),
			.module = create_shader_module(ctx->device(), *shader_info),
			.pName = shader_info->entry_point.c_str(),
			.pSpecializationInfo = nullptr
		};
		shader_infos.push_back(ssci);
	}
	gpci.stageCount = shader_infos.size();
	gpci.pStages = shader_infos.data();

	Pipeline pipeline;
	vkCreateGraphicsPipelines(ctx->device(), nullptr, 1, &gpci, nullptr, &pipeline.handle);
	pipeline.name = pci.name;
	pipeline.layout = layout;
	pipeline.type = PipelineType::Graphics;

	ctx->name_object(pipeline, "[Gfx Pipeline] " + pci.name);

	// Delete the creates shader modules. We might want to cache these (TODO)
	for (auto& ssci : shader_infos) {
		vkDestroyShaderModule(ctx->device(), ssci.module, nullptr);
	}

	this->pipeline.insert(pci, pipeline);

	return pipeline;
}

Pipeline CacheImpl::get_or_create_compute_pipeline(ph::ComputePipelineCreateInfo& pci) {
	{
		Pipeline* pipeline = this->compute_pipeline.get(pci);
		if (pipeline) { return *pipeline; }
	}

	VkComputePipelineCreateInfo cpci{};
	VkDescriptorSetLayout set_layout = get_or_create_descriptor_set_layout(pci.layout.set_layout);
	ph::PipelineLayout layout = get_or_create_pipeline_layout(pci.layout, set_layout);
	cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	// Create shader modules
	ph::ShaderModuleCreateInfo* shader_info = this->shader.get(pci.shader);
	assert(shader_info && "Invalid shader handle");
	VkPipelineShaderStageCreateInfo ssci{
		.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
		.pNext = nullptr,
		.flags = {},
		.stage = VK_SHADER_STAGE_COMPUTE_BIT,
		.module = create_shader_module(ctx->device(), *shader_info),
		.pName = shader_info->entry_point.c_str(),
		.pSpecializationInfo = nullptr
	};
	cpci.stage = ssci;
	cpci.layout = layout.handle;
	Pipeline pipeline;
	vkCreateComputePipelines(ctx->device(), nullptr, 1, &cpci, nullptr, &pipeline.handle);
	pipeline.name = pci.name;
	pipeline.layout = layout;
	pipeline.type = PipelineType::Compute;
	ctx->name_object(pipeline, "[Compute Pipeline] " + pci.name);
	vkDestroyShaderModule(ctx->device(), ssci.module, nullptr);

	this->compute_pipeline.insert(pci, pipeline);

	return pipeline;
}

#if PHOBOS_ENABLE_RAY_TRACING

#define PH_RTX_CALL(func, ...) ctx->rtx_fun._##func(__VA_ARGS__)

Pipeline CacheImpl::get_or_create_ray_tracing_pipeline(ph::RayTracingPipelineCreateInfo& pci) {
	{
		Pipeline* pipeline = this->rtx_pipeline.get(pci);
		if (pipeline) { return *pipeline; }
	}

	VkRayTracingPipelineCreateInfoKHR rtpci{};
	rtpci.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	VkDescriptorSetLayout set_layout = get_or_create_descriptor_set_layout(pci.layout.set_layout);
	ph::PipelineLayout layout = get_or_create_pipeline_layout(pci.layout, set_layout);
	rtpci.layout = layout.handle;

	std::vector<VkPipelineShaderStageCreateInfo> sscis{};
	// Used for creating the shader groups in the next step
	std::unordered_map<ShaderHandle, uint32_t> shader_indices;
	for (uint32_t i = 0; i < pci.shaders.size(); ++i) {
		ShaderHandle shader_handle = pci.shaders[i];
		shader_indices[shader_handle] = i;

		ph::ShaderModuleCreateInfo* shader = this->shader.get(shader_handle);
		assert(shader && "Invalid shader handle");
		sscis.push_back(VkPipelineShaderStageCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = nullptr,
			.flags = {},
			.stage = static_cast<VkShaderStageFlagBits>(shader->stage),
			.module = create_shader_module(ctx->device(), *shader),
			.pName = shader->entry_point.c_str()
		});
	}

	// Now we create the shader groups
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
	for (RayTracingShaderGroup& group : pci.shader_groups) {
		groups.push_back(VkRayTracingShaderGroupCreateInfoKHR{
			.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
			.pNext = nullptr,
			.type = group.type,
			.generalShader = (group.general.id != ShaderHandle::none) ? shader_indices[group.general] : VK_SHADER_UNUSED_KHR,
			.closestHitShader = (group.closest_hit.id != ShaderHandle::none) ? shader_indices[group.closest_hit] : VK_SHADER_UNUSED_KHR,
			.anyHitShader = (group.any_hit.id != ShaderHandle::none) ? shader_indices[group.any_hit] : VK_SHADER_UNUSED_KHR,
			.intersectionShader = (group.intersection.id != ShaderHandle::none) ? shader_indices[group.intersection] : VK_SHADER_UNUSED_KHR,
			.pShaderGroupCaptureReplayHandle = nullptr
		});
	}

	// We can now create the RT pipeline.
	rtpci.stageCount = sscis.size();
	rtpci.pStages = sscis.data();
	rtpci.groupCount = groups.size();
	rtpci.pGroups = groups.data();

	Pipeline pipeline;
	PH_RTX_CALL(vkCreateRayTracingPipelinesKHR, ctx->device(), {}, {}, 1, &rtpci, nullptr, &pipeline.handle);
	pipeline.name = pci.name;
	pipeline.type = ph::PipelineType::RayTracing;
	pipeline.layout = layout;
	ctx->name_object(pipeline, "[RTX Pipeline] " + pci.name);
	// Destroy shader modules
	for (auto& shader : sscis) {
		vkDestroyShaderModule(ctx->device(), shader.module, nullptr);
	}
	this->rtx_pipeline.insert(pci, pipeline);

	return pipeline;
}

#endif

VkDescriptorSet CacheImpl::get_or_create_descriptor_set(DescriptorSetBinding set_binding, Pipeline const& pipeline, void* pNext) {
	set_binding.set_layout = pipeline.layout.set_layout;
	auto set_opt = this->descriptor_set.current().get(set_binding);
	if (set_opt) {
		return *set_opt;
	}
	// Create descriptor set layout and issue writes
	VkDescriptorSetAllocateInfo alloc_info;
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.pSetLayouts = &set_binding.set_layout;
	// add default descriptor pool if no custom one was specified
	if (!set_binding.pool) { set_binding.pool = descr_pool; };
	alloc_info.descriptorPool = set_binding.pool;
	alloc_info.descriptorSetCount = 1;
	alloc_info.pNext = pNext;

	VkDescriptorSet set = nullptr;
	vkAllocateDescriptorSets(ctx->device(), &alloc_info, &set);

	// Now we have the set we need to write the requested data to it
	std::vector<VkWriteDescriptorSet> writes;
	struct DescriptorWriteInfo {
		std::vector<VkDescriptorBufferInfo> buffer_infos;
		std::vector<VkDescriptorImageInfo> image_infos;
#if PHOBOS_ENABLE_RAY_TRACING
		std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accel_infos;
#endif
	};
	std::vector<DescriptorWriteInfo> write_infos;
	writes.reserve(set_binding.bindings.size());
	for (auto const& binding : set_binding.bindings) {
		if (binding.descriptors.empty()) continue;
		DescriptorWriteInfo write_info;
		VkWriteDescriptorSet write{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.pNext = nullptr
		};
		write.dstSet = set;
		write.dstBinding = binding.binding;
		write.descriptorType = binding.type;
		write.descriptorCount = binding.descriptors.size();
		switch (binding.type) {
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
			write_info.image_infos.reserve(binding.descriptors.size());
			for (auto const& descriptor : binding.descriptors) {
				VkDescriptorImageInfo img{
					.sampler = descriptor.image.sampler,
					.imageView = descriptor.image.view.handle,
					.imageLayout = descriptor.image.layout
				};
				write_info.image_infos.push_back(img);
				// Push dummy buffer info to make sure indices match
				write_info.buffer_infos.emplace_back();
#if PHOBOS_ENABLE_RAY_TRACING
				write_info.accel_infos.emplace_back();
#endif
			}
		} break;
#if PHOBOS_ENABLE_RAY_TRACING
		case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
			auto& info = binding.descriptors.front().accel_structure;
			VkWriteDescriptorSetAccelerationStructureKHR as{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
				.pNext = nullptr,
				.accelerationStructureCount = 1,
				.pAccelerationStructures = &info.structure
			};
			write_info.accel_infos.push_back(as);
			write_info.buffer_infos.emplace_back();
			write_info.image_infos.emplace_back();
		} break;
#endif
		default: {
			auto& info = binding.descriptors.front().buffer;
			VkDescriptorBufferInfo buf{
				.buffer = info.buffer,
				.offset = info.offset,
				.range = info.range
			};

			write_info.buffer_infos.push_back(buf);
			// Push dummy image info to make sure indices match
			write_info.image_infos.emplace_back();
#if PHOBOS_ENABLE_RAY_TRACING
			write_info.accel_infos.emplace_back();
#endif
		} break;
		}
		write_infos.push_back(write_info);
		writes.push_back(write);
	}

	for (size_t i = 0; i < write_infos.size(); ++i) {
		switch (writes[i].descriptorType) {
		case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
		case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
		case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE: {
			writes[i].pImageInfo = write_infos[i].image_infos.data();
		} break;
#if PHOBOS_ENABLE_RAY_TRACING
		case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: {
			writes[i].pNext = write_infos[i].accel_infos.data();
		} break;
#endif
		default: {
			writes[i].pBufferInfo = write_infos[i].buffer_infos.data();
		} break;
		}
	}

	vkUpdateDescriptorSets(ctx->device(), writes.size(), writes.data(), 0, nullptr);
	this->descriptor_set.current().insert(set_binding, set);
	return set;
}

void CacheImpl::next_frame() {
	this->descriptor_set.next();
}

}
}