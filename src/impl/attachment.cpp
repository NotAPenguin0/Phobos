// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Attachment management

#include <phobos/impl/context.hpp>
#include <phobos/impl/attachment.hpp>
#include <phobos/impl/image.hpp>

using namespace std::literals::string_literals;

namespace ph {
namespace impl {

AttachmentImpl::AttachmentImpl(ContextImpl& ctx, ImageImpl& img) : ctx(&ctx), img(&img) {
	
}

AttachmentImpl::~AttachmentImpl() {
	for (auto& [name, attachment] : attachments) {
		// Don't destroy swapchain attachment reference, as we don't own it and it will be destroyed later
		if (name != swapchain_attachment_name) {
			img->destroy_image_view(attachment.attachment.view);
			if (attachment.image) {
				img->destroy_image(*attachment.image);
			}
		}
	}
}


Attachment* AttachmentImpl::get_attachment(std::string_view name) {
	std::string key{ name };
	if (auto it = attachments.find(key); it != attachments.end()) {
		return &it->second.attachment;
	}
	return nullptr;
}

bool AttachmentImpl::is_attachment(ImageView view) {
	auto it = std::find_if(attachments.begin(), attachments.end(), [view](auto const& att) {
		return att.second.attachment.view.id == view.id;
	});
	if (it != attachments.end()) return true;
	return false;
}

std::string AttachmentImpl::get_attachment_name(ImageView view) {
	auto it = std::find_if(attachments.begin(), attachments.end(), [view](auto const& att) {
		return att.second.attachment.view.id == view.id;
		});
	if (it != attachments.end()) return it->first;
	return "";
}

void AttachmentImpl::create_attachment(std::string_view name, VkExtent2D size, VkFormat format, ImageType type) {
    create_attachment(name, size, format, VK_SAMPLE_COUNT_1_BIT, type);
}

void AttachmentImpl::create_attachment(std::string_view name, VkExtent2D size, VkFormat format, VkSampleCountFlagBits samples, ImageType type) {
    InternalAttachment attachment{};
    // Create image and image view
    attachment.image = img->create_image(type, size, format, samples);
    attachment.attachment.view = img->create_image_view(*attachment.image, is_depth_format(format) ? ImageAspect::Depth : ImageAspect::Color);
    attachments[std::string{ name }] = attachment;

    ctx->name_object(attachment.image->handle, name.data() + " - image"s);
    ctx->name_object(attachment.attachment.view, name.data() + " - view"s);
}

void AttachmentImpl::resize_attachment(std::string_view name, VkExtent2D new_size) {
	Attachment* att = get_attachment(name);
	if (att == nullptr) {
		ctx->log(ph::LogSeverity::Error, "Tried to resize nonexistent attachment");
		return;
	}

	// No resize needed
	if (att->view.size.width == new_size.width && att->view.size.height == new_size.height) {
		return;
	}

	// Lookup internal attachment
	InternalAttachment& data = attachments.at(std::string(name));
	// Prepare old data for deferred deletion
	deferred_delete.push_back({ .attachment = data, .frames_left = ctx->max_frames_in_flight + 2 }); // Might be too many frames, but the extra safety doesn't hurt.
	// Create new attachment
	VkFormat format = att->view.format;
    VkSampleCountFlagBits samples = att->view.samples;
	data.image = img->create_image(data.image->type, new_size, format, samples);
	data.attachment.view = img->create_image_view(*data.image, is_depth_format(format) ? ImageAspect::Depth : ImageAspect::Color);

    ctx->name_object(data.image->handle, name.data() + " - image"s);
    ctx->name_object(data.attachment.view, name.data() + " - view"s);
}

bool AttachmentImpl::is_swapchain_attachment(std::string const& name) {
	return name == swapchain_attachment_name;
}

std::string AttachmentImpl::get_swapchain_attachment_name() const {
	return swapchain_attachment_name;
}


void AttachmentImpl::new_frame(ImageView& swapchain_view) {
	attachments[swapchain_attachment_name] = InternalAttachment{ Attachment{.view = swapchain_view }, std::nullopt };

	// Update deferred deletion list
	for (auto& deferred : deferred_delete) {
		deferred.frames_left -= 1;
		// Lifetime expired, deleting
		if (deferred.frames_left == 0) {
			img->destroy_image_view(deferred.attachment.attachment.view);
			img->destroy_image(*deferred.attachment.image);
		}
	}

	// Remove expired entries
	deferred_delete.erase(
		std::remove_if(deferred_delete.begin(), deferred_delete.end(), 
			[](DeferredDelete const& entry) {
				return entry.frames_left == 0; 
			}), 
		deferred_delete.end());
}

} // namespace impl
} // namespace ph