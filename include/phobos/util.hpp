#ifndef PHOBOS_UTIL_HPP_
#define PHOBOS_UTIL_HPP_

#include <vulkan/vulkan.hpp>

namespace ph {

vk::ImageView create_image_view(vk::Device device, vk::Image image, vk::Format format);

}

#endif