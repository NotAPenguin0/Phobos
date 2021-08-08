#pragma once

#include <phobos/context.hpp>

namespace ph {
namespace impl {

class AttachmentImpl {
public:
	AttachmentImpl(ContextImpl& ctx, ImageImpl& image);
	~AttachmentImpl();

	// PUBLIC API FUNCTIONS

	Attachment* get_attachment(std::string_view name);
	void create_attachment(std::string_view name, VkExtent2D size, VkFormat format);
	bool is_swapchain_attachment(std::string const& name);
	bool is_attachment(ImageView view);
	std::string get_swapchain_attachment_name() const;

	// PRIVATE IMPLEMENTATION FUNCTIONS (these are public since they can be used by other impl classes freely)

	// Called at the beginning of a frame by the frame implementation to set the swapchain attachment to point to the correct image view.
	void update_swapchain_attachment(ImageView& view);

private:
	ContextImpl* ctx;
	ImageImpl* img;

	static inline std::string swapchain_attachment_name = "swapchain";

	struct InternalAttachment {
		Attachment attachment;
		std::optional<RawImage> image;
	};
	std::unordered_map<std::string, InternalAttachment> attachments{};
};

}
}