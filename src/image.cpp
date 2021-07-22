#include <phobos/image.hpp>

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

} // namespace ph
