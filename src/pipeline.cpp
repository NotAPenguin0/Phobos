#include <phobos/pipeline.hpp>
#include <phobos/context.hpp>
#include <phobos/memory.hpp>

namespace ph {

DescriptorBuilder DescriptorBuilder::create(Context& ctx, Pipeline const& pipeline) {
	DescriptorBuilder builder{};
	builder.ctx = &ctx;
	builder.pipeline = pipeline;
	return builder;
}

DescriptorBuilder& DescriptorBuilder::add_pNext(void* p) {
	pNext_chain.push_back(p);
	return *this;
}

DescriptorBuilder& DescriptorBuilder::add_sampled_image(uint32_t binding, ImageView view, VkSampler sampler, VkImageLayout layout) {
	DescriptorBinding descr{};
	descr.binding = binding;
	descr.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	auto& descriptor = descr.descriptors.emplace_back();
	descriptor.image = ph::DescriptorImageInfo{
		.sampler = sampler,
		.view = view,
		.layout = layout
	};
	info.bindings.push_back(std::move(descr));
	return *this;
}

DescriptorBuilder& DescriptorBuilder::add_sampled_image(ShaderMeta::Binding const& binding, ImageView view, VkSampler sampler, VkImageLayout layout) {
	return add_sampled_image(binding.binding, view, sampler, layout);
}

DescriptorBuilder& DescriptorBuilder::add_sampled_image(std::string_view binding, ImageView view, VkSampler sampler, VkImageLayout layout) {
	return add_sampled_image(ctx->get_shader_meta(pipeline)[binding], view, sampler, layout);
}

DescriptorBuilder& DescriptorBuilder::add_storage_image(uint32_t binding, ImageView view, VkImageLayout layout) {
	DescriptorBinding descr{};
	descr.binding = binding;
	descr.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	auto& descriptor = descr.descriptors.emplace_back();
	descriptor.image = ph::DescriptorImageInfo{
		.view = view,
		.layout = layout
	};
	info.bindings.push_back(std::move(descr));
	return *this;
}

DescriptorBuilder& DescriptorBuilder::add_storage_image(ShaderMeta::Binding const& binding, ImageView view, VkImageLayout layout) {
	return add_storage_image(binding.binding, view, layout);
}

DescriptorBuilder& DescriptorBuilder::add_storage_image(std::string_view binding, ImageView view, VkImageLayout layout) {
	return add_storage_image(ctx->get_shader_meta(pipeline)[binding], view, layout);
}

DescriptorBuilder& DescriptorBuilder::add_uniform_buffer(uint32_t binding, BufferSlice buffer) {
	DescriptorBinding descr{};
	descr.binding = binding;
	descr.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	auto& descriptor = descr.descriptors.emplace_back();
	descriptor.buffer = ph::DescriptorBufferInfo{
		.buffer = buffer.buffer,
		.offset = buffer.offset,
		.range = buffer.range
	};
	info.bindings.push_back(std::move(descr));
	return *this;
}

DescriptorBuilder& DescriptorBuilder::add_uniform_buffer(ShaderMeta::Binding const& binding, BufferSlice buffer) {
	return add_uniform_buffer(binding.binding, buffer);
}

DescriptorBuilder& DescriptorBuilder::add_uniform_buffer(std::string_view binding, BufferSlice buffer) {
	return add_uniform_buffer(ctx->get_shader_meta(pipeline)[binding], buffer);
}

DescriptorBuilder& DescriptorBuilder::add_storage_buffer(uint32_t binding, BufferSlice buffer) {
	DescriptorBinding descr{};
	descr.binding = binding;
	descr.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	auto& descriptor = descr.descriptors.emplace_back();
	descriptor.buffer = ph::DescriptorBufferInfo{
		.buffer = buffer.buffer,
		.offset = buffer.offset,
		.range = buffer.range
	};
	info.bindings.push_back(std::move(descr));
	return *this;
}

DescriptorBuilder& DescriptorBuilder::add_storage_buffer(ShaderMeta::Binding const& binding, BufferSlice buffer) {
	return add_storage_buffer(binding.binding, buffer);
}

DescriptorBuilder& DescriptorBuilder::add_storage_buffer(std::string_view binding, BufferSlice buffer) {
	return add_storage_buffer(ctx->get_shader_meta(pipeline)[binding], buffer);
}

VkDescriptorSet DescriptorBuilder::get() {
	void* pNext = nullptr;
	if (!pNext_chain.empty()) {
		pNext = pNext_chain.front();
		VkBaseOutStructure* cur = reinterpret_cast<VkBaseOutStructure*>(pNext);
		for (size_t i = 1; i < pNext_chain.size(); ++i) {
			cur->pNext = reinterpret_cast<VkBaseOutStructure*>(pNext_chain[i]);
			cur = cur->pNext;
		}
	}
	return ctx->get_or_create(info, pipeline, pNext);
}

PipelineBuilder PipelineBuilder::create(Context& ctx, std::string_view name) {
	PipelineBuilder builder{};
	builder.ctx = &ctx;
	builder.pci.name = name;
	return builder;
}

// TODO: ALIGN OFFSET PROPERLY

PipelineBuilder& PipelineBuilder::add_vertex_input(uint32_t binding, VkVertexInputRate input_rate) {
	vertex_binding_offsets[binding] = 0;
	// Each time a vertex attribute is added, the stride variable is updated
	pci.vertex_input_bindings.push_back(VkVertexInputBindingDescription{
		.binding = binding,
		.stride = 0,
		.inputRate = input_rate
	});
	return *this;
}

PipelineBuilder& PipelineBuilder::add_vertex_attribute(uint32_t binding, uint32_t location, VkFormat format) {
	uint32_t size = format_byte_size(format);
	uint32_t offset = vertex_binding_offsets[binding];
	pci.vertex_attributes.push_back(VkVertexInputAttributeDescription{
		.location = location,
		.binding = binding,
		.format = format,
		.offset = offset
	});
	// Update offset and stride
	vertex_binding_offsets[binding] += size;
	for (auto& binding : pci.vertex_input_bindings) {
		binding.stride += size;
	}
	return *this;
}

PipelineBuilder& PipelineBuilder::add_shader(std::string_view path, std::string_view entry, PipelineStage stage) {
	add_shader(ctx->create_shader(path, entry, stage));
	return *this;
}

PipelineBuilder& PipelineBuilder::add_shader(ShaderHandle shader) {
	pci.shaders.push_back(shader);
	return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_test(bool test) {
	pci.depth_stencil.depthTestEnable = test;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_write(bool write) {
	pci.depth_stencil.depthWriteEnable = write;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_depth_op(VkCompareOp op) {
	pci.depth_stencil.depthCompareOp = op;
	return *this;
}

PipelineBuilder& PipelineBuilder::add_dynamic_state(VkDynamicState state) {
	pci.dynamic_states.push_back(state);
	if (state == VK_DYNAMIC_STATE_VIEWPORT) return add_viewport(VkViewport{});
	if (state == VK_DYNAMIC_STATE_SCISSOR) return add_scissor(VkRect2D{});
	return *this;
}

PipelineBuilder& PipelineBuilder::set_polygon_mode(VkPolygonMode mode) {
	pci.rasterizer.polygonMode = mode;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_cull_mode(VkCullModeFlags mode) {
	pci.rasterizer.cullMode = mode;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_front_face(VkFrontFace face) {
	pci.rasterizer.frontFace = face;
	return *this;
}

PipelineBuilder& PipelineBuilder::set_samples(VkSampleCountFlagBits samples) {
	pci.multisample.rasterizationSamples = samples;
	return *this;
}

PipelineBuilder& PipelineBuilder::add_blend_attachment(bool enable, 
	VkBlendFactor src_color_factor, VkBlendFactor dst_color_factor, VkBlendOp color_op,
	VkBlendFactor src_alpha_factor, VkBlendFactor dst_alpha_factor, VkBlendOp alpha_op, 
	VkColorComponentFlags write_mask) {
	pci.blend_attachments.push_back(VkPipelineColorBlendAttachmentState{
		.blendEnable = enable,
		.srcColorBlendFactor = src_color_factor, .dstColorBlendFactor = dst_color_factor, .colorBlendOp = color_op,
		.srcAlphaBlendFactor = src_alpha_factor, .dstAlphaBlendFactor = dst_alpha_factor, .alphaBlendOp = alpha_op,
		.colorWriteMask = write_mask
	});
	return *this;
}

PipelineBuilder& PipelineBuilder::add_viewport(VkViewport vp) {
	pci.viewports.push_back(vp);
	return *this;
}

PipelineBuilder& PipelineBuilder::add_scissor(VkRect2D scissor) {
	pci.scissors.push_back(scissor);
	return *this;
}

PipelineBuilder& PipelineBuilder::enable_blend_op(bool enable) {
	pci.blend_logic_op_enable = enable;
	return *this;
}

PipelineBuilder& PipelineBuilder::reflect() {
	ctx->reflect_shaders(pci);
	return *this;
}

PipelineCreateInfo PipelineBuilder::get() {
	return std::move(pci);
}

ComputePipelineBuilder ComputePipelineBuilder::create(Context& ctx, std::string_view name) {
	ComputePipelineBuilder builder{};
	builder.ctx = &ctx;
	builder.pci.name = name;
	return builder;
}

ComputePipelineBuilder& ComputePipelineBuilder::set_shader(ShaderHandle shader) {
	pci.shader = shader;
	return *this;
}

ComputePipelineBuilder& ComputePipelineBuilder::set_shader(std::string_view path, std::string_view entry) {
	return set_shader(ctx->create_shader(path, entry, ph::PipelineStage::ComputeShader));
}

ComputePipelineBuilder& ComputePipelineBuilder::reflect() {
	ctx->reflect_shaders(pci);
	return *this;
}


ph::ComputePipelineCreateInfo ComputePipelineBuilder::get() {
	return std::move(pci);
}

}