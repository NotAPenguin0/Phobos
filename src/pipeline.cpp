#include <phobos/pipeline.hpp>
#include <phobos/context.hpp>
#include <phobos/memory.hpp>

namespace ph {

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

}