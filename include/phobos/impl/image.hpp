#pragma once 

#include <phobos/context.hpp>

#include <unordered_map>
#include <mutex>

namespace ph {
namespace impl {

class ImageImpl {
public:
	ImageImpl(ContextImpl& ctx);

	// PUBLIC API

	RawImage create_image(ImageType type, VkExtent2D size, VkFormat format, uint32_t mips = 1);
    RawImage create_image(ImageType type, VkExtent2D size, VkFormat format, VkSampleCountFlagBits samples, uint32_t mips = 1);
	void destroy_image(RawImage& image);

	ImageView create_image_view(RawImage const& target, ImageAspect aspect = ImageAspect::Color);
    ImageView create_image_view(RawImage const& target, uint32_t mip, ImageAspect aspect = ImageAspect::Color);
	void destroy_image_view(ImageView& view);

	ImageView get_image_view(uint64_t id);

	// PRIVATE API

private:
	ContextImpl* ctx;

	mutable std::mutex mutex{};

	/**
	 * @brief We keep track of all image views so that they can be requested by ID.
	*/
	std::unordered_map<uint64_t /*id*/, ImageView> all_image_views;
};

}
}