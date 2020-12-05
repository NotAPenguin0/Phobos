#include <phobos/memory.hpp>

namespace ph {

uint32_t format_byte_size(VkFormat format) {
	switch (format) {
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
		return 4;
	case VK_FORMAT_R8G8B8_UNORM:
	case VK_FORMAT_R8G8B8_SRGB:
		return 3;
	case VK_FORMAT_R32G32B32A32_SFLOAT:
		return 4 * 4;
	case VK_FORMAT_R32G32B32_SFLOAT:
		return 3 * 4;
	case VK_FORMAT_R32G32_SFLOAT:
		return 2 * 4;
	default:
		return 0;
	}
}

}