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

#if PHOBOS_ENABLE_RAY_TRACING
PassBuilder PassBuilder::create_ray_tracing(std::string_view name) {
	PassBuilder builder{};
	builder.pass.name = name;
	builder.pass.no_renderpass = true;
	return builder;
}
#endif

PassBuilder& PassBuilder::add_attachment(std::string_view name, LoadOp load_op, ClearValue clear) {
    return add_attachment(name, {}, load_op, clear);
}

PassBuilder& PassBuilder::add_attachment(std::string_view name, ImageView view, LoadOp load_op, ClearValue clear) {
    ResourceUsage usage;
    usage.type = ResourceType::Attachment;
    usage.access = ResourceAccess::ColorAttachmentOutput;
    usage.stage = PipelineStage::AttachmentOutput;
    usage.attachment.name = name;
    usage.attachment.load_op = load_op;
    usage.attachment.clear = clear;
    usage.attachment.view = view;
    pass.resources.push_back(std::move(usage));
    return *this;
}

PassBuilder& PassBuilder::add_depth_attachment(std::string_view name, LoadOp load_op, ClearValue clear) {
    return add_depth_attachment(name, {}, load_op, clear);
}

PassBuilder& PassBuilder::add_depth_attachment(std::string_view name, ImageView view, LoadOp load_op, ClearValue clear) {
    ResourceUsage usage;
    usage.type = ResourceType::Attachment;
    usage.access = ResourceAccess::DepthStencilAttachmentOutput;
    usage.stage = PipelineStage::AttachmentOutput;
    usage.attachment.name = name;
    usage.attachment.load_op = load_op;
    usage.attachment.clear = clear;
    usage.attachment.view = view;
    pass.resources.push_back(std::move(usage));
    return *this;
}

PassBuilder& PassBuilder::sample_attachment(std::string_view name, plib::bit_flag<PipelineStage> stage) {
    return sample_attachment(name, {}, stage);
}

PassBuilder& PassBuilder::sample_attachment(std::string_view name, ImageView view, plib::bit_flag<PipelineStage> stage) {
    ResourceUsage usage{};
    usage.type = ResourceType::Attachment;
    usage.access = ResourceAccess::ShaderRead;
    usage.stage = stage;
    usage.attachment.name = name;
    usage.attachment.view = view;
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

	// 'Collapse' usages by finding usage structs with the same resource but different usage flags.
	// Add the usage flags together into one struct and replace

	struct ResourceFinder {
		ResourceUsage const& a;

		bool operator()(ResourceUsage const& b) const {
			if (a.stage != b.stage) return false;

			if (a.type == ResourceType::Attachment && b.type == ResourceType::Attachment) {
				return a.attachment.name == b.attachment.name;
			}
			if (a.type == ResourceType::Buffer && b.type == ResourceType::Buffer) {
				return a.buffer.slice == b.buffer.slice;
			}
			if (a.type == ResourceType::Image && b.type == ResourceType::Image) {
				return a.image.view == b.image.view;
			}
			if (a.type == ResourceType::StorageImage && b.type == ResourceType::StorageImage) {
				return a.image.view == b.image.view;
			}

			return false;
		}
	};

	std::vector<ResourceUsage> resources_final{};
	for (auto& resource : pass.resources) {
		// If this resource is already in the resources list, add it's usage flag.
		auto it = std::find_if(resources_final.begin(), resources_final.end(), ResourceFinder{ resource });
		if (it != resources_final.end()) {
			it->access |= resource.access;
		}
		// It's not in the resources list yet, simply add it by copying it over
		else {
			resources_final.push_back(resource);
		}
	}
	// Swap the pass resources with the resulting resource list.
	pass.resources = std::move(resources_final);

	return std::move(pass);
}

}