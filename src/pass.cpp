#include <phobos/pass.hpp>

namespace ph {

PassBuilder PassBuilder::create(std::string_view name) {
	PassBuilder builder{};
	builder.pass.name = name;
	return builder;
}

PassBuilder PassBuilder::create_compute(std::string_view name) {
	PassBuilder builder{};
	builder.pass.name = name;
	builder.pass.no_renderpass = true;
	return builder;
}

PassBuilder PassBuilder::create_ray_tracing(std::string_view name) {
	PassBuilder builder{};
	builder.pass.name = name;
	builder.pass.no_renderpass = true;
	return builder;
}

PassBuilder& PassBuilder::add_attachment(std::string_view name, LoadOp load_op, ClearValue clear) {
	ResourceUsage usage;
	usage.type = ResourceType::Attachment;
	usage.access = ResourceAccess::ColorAttachmentOutput;
	usage.stage = PipelineStage::AttachmentOutput;
	usage.attachment.name = name;
	usage.attachment.load_op = load_op;
	usage.attachment.clear = clear;
	pass.resources.push_back(std::move(usage));
	return *this;
}

PassBuilder& PassBuilder::add_depth_attachment(std::string_view name, LoadOp load_op, ClearValue clear) {
	ResourceUsage usage;
	usage.type = ResourceType::Attachment;
	usage.access = ResourceAccess::DepthStencilAttachmentOutput;
	usage.stage = PipelineStage::AttachmentOutput;
	usage.attachment.name = name;
	usage.attachment.load_op = load_op;
	usage.attachment.clear = clear;
	pass.resources.push_back(std::move(usage));
	return *this;
}

PassBuilder& PassBuilder::sample_attachment(std::string_view name, plib::bit_flag<PipelineStage> stage) {
	ResourceUsage usage{};
	usage.type = ResourceType::Attachment;
	usage.access = ResourceAccess::ShaderRead;
	usage.stage = stage;
	usage.attachment.name = name;
	pass.resources.push_back(std::move(usage));
	return *this;
}

PassBuilder& PassBuilder::shader_read_buffer(BufferSlice slice, plib::bit_flag<PipelineStage> stage) {
	ResourceUsage usage{};
	usage.type = ResourceType::Buffer;
	usage.access = ResourceAccess::ShaderRead;
	usage.stage = stage;
	usage.buffer.slice = slice;
	pass.resources.push_back(usage);
	return *this;
}

PassBuilder& PassBuilder::shader_write_buffer(BufferSlice slice, plib::bit_flag<PipelineStage> stage) {
	ResourceUsage usage{};
	usage.type = ResourceType::Buffer;
	usage.access = ResourceAccess::ShaderWrite;
	usage.stage = stage;
	usage.buffer.slice = slice;
	pass.resources.push_back(usage);
	return *this;
}

PassBuilder& PassBuilder::write_storage_image(ImageView view, plib::bit_flag<PipelineStage> stage) {
	ResourceUsage usage{};
	usage.type = ResourceType::StorageImage;
	usage.access = ResourceAccess::ShaderWrite;
	usage.stage = stage;
	usage.image.view = view;
	pass.resources.push_back(usage);
	return *this;
}

PassBuilder& PassBuilder::read_storage_image(ImageView view, plib::bit_flag<PipelineStage> stage) {
	ResourceUsage usage{};
	usage.type = ResourceType::StorageImage;
	usage.access = ResourceAccess::ShaderRead;
	usage.stage = stage;
	usage.image.view = view;
	pass.resources.push_back(usage);
	return *this;
}

PassBuilder& PassBuilder::sample_image(ImageView view, plib::bit_flag<PipelineStage> stage) {
	ResourceUsage usage{};
	usage.type = ResourceType::Image;
	usage.access = ResourceAccess::ShaderRead;
	usage.stage = stage;
	usage.image.view = view;
	pass.resources.push_back(usage);
	return *this;
}

PassBuilder& PassBuilder::execute(std::function<void(ph::CommandBuffer&)> callback) {
	pass.execute = std::move(callback);
	return *this;
}

Pass PassBuilder::get() {
	return std::move(pass);
}

}