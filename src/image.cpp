#include <phobos/image.hpp>

#include <cassert>

namespace ph {

bool is_depth_format(VkFormat format) {
    switch (format) {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_D32_SFLOAT:
        return true;
    default:
        return false;
    }
}

VkDeviceSize format_size(VkFormat format) {
	switch (format) {
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SRGB:
			return 4;
		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SRGB:
			return 3;
        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SRGB:
            return 2;
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SRGB:
            return 1;
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return 4 * sizeof(float);
		case VK_FORMAT_R32G32B32_SFLOAT:
			return 3 * sizeof(float);

		default:
			assert(false && "Invalid format!");
	}
}

} // namespace ph
