// CONTEXT IMPLEMENTATION FILE
// CONTENTS: 
//		- Attachment management

#include <phobos/impl/context.hpp>
#include <phobos/impl/attachment.hpp>
#include <phobos/impl/image.hpp>

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

void AttachmentImpl::create_attachment(std::string_view name, VkExtent2D size, VkFormat format) {
	InternalAttachment attachment{};
	// Create image and image view
	attachment.image = img->create_image(is_depth_format(format) ? ImageType::DepthStencilAttachment : ImageType::ColorAttachment, size, format);
	attachment.attachment.view = img->create_image_view(*attachment.image, is_depth_format(format) ? ImageAspect::Depth : ImageAspect::Color);
	attachments[std::string{ name }] = attachment;
}

bool AttachmentImpl::is_swapchain_attachment(std::string const& name) {
	return name == swapchain_attachment_name;
}

std::string AttachmentImpl::get_swapchain_attachment_name() const {
	return swapchain_attachment_name;
}


void AttachmentImpl::update_swapchain_attachment(ImageView& view) {
	attachments[swapchain_attachment_name] = InternalAttachment{ Attachment{.view = view }, std::nullopt };
}

} // namespace impl
} // namespace ph