#pragma once

#include <phobos/context.hpp>

namespace ph {
namespace impl {

class AttachmentImpl {
public:
	AttachmentImpl(ContextImpl& ctx, ImageImpl& image);
	~AttachmentImpl();

	// PUBLIC API FUNCTIONS

	Attachment get_attachment(std::string_view name);
	void create_attachment(std::string_view name, VkExtent2D size, VkFormat format, ImageType type);
    void create_attachment(std::string_view name, VkExtent2D size, VkFormat format, VkSampleCountFlagBits samples, ImageType type);
	void resize_attachment(std::string_view name, VkExtent2D new_size);
	bool is_swapchain_attachment(std::string const& name);
	bool is_attachment(ImageView view);
	std::string get_attachment_name(ImageView view);
	std::string get_swapchain_attachment_name() const;

	// PRIVATE IMPLEMENTATION FUNCTIONS (these are public since they can be used by other impl classes freely)

	// Called at the beginning of a frame by the frame implementation to set the swapchain attachment to point to the correct image view.
	void new_frame(ImageView& swapchain_view);

private:
	ContextImpl* ctx;
	ImageImpl* img;

	static inline std::string swapchain_attachment_name = "swapchain";

    // Note that the only difference between this and Attachment is the const-ness of the image handle.
	struct InternalAttachment {
		ph::ImageView view;
		std::optional<RawImage> image;
	};
	std::unordered_map<std::string, InternalAttachment> attachments{};

	struct DeferredDelete {
		InternalAttachment attachment;
		uint32_t frames_left = 0;
	};
	std::vector<DeferredDelete> deferred_delete{};
};

}
}