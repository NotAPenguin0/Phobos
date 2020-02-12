#ifndef PHOBOS_SHADER_UTIL_HPP_
#define PHOBOS_SHADER_UTIL_HPP_

#include <vulkan/vulkan.hpp>
#include <string_view>

namespace ph {

// TODO: Avoid recreating the same ShaderModule twice

vk::ShaderModule load_shader(vk::Device device, std::string_view file_path);


}

#endif