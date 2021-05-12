#pragma once 

#include <phobos/context.hpp>

namespace ph {
namespace impl {

class ImageImpl {
public:
	ImageImpl(ContextImpl& ctx);

	// PUBLIC API

	RawImage create_image(ImageType type, VkExtent2D size, VkFormat format);
	void destroy_image(RawImage& image);

	ImageView create_image_view(RawImage const& target, ImageAspect aspect = ImageAspect::Color);
	void destroy_image_view(ImageView& view);

	// PRIVATE API

private:
	ContextImpl* ctx;
};

}
}