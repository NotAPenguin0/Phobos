#ifndef PHOBOS_SHADER_UTIL_HPP_
#define PHOBOS_SHADER_UTIL_HPP_

#include <string_view>
#include <vector>

#include <phobos/forward.hpp>
#include <vulkan/vulkan.hpp>

namespace ph {

struct ShaderHandle {
	static constexpr uint32_t none = static_cast<uint32_t>(-1);

	uint32_t id = none;
};

std::vector<uint32_t> load_shader_code(std::string_view file_path);

ShaderHandle create_shader(VulkanContext& ctx, std::vector<uint32_t> const& code, std::string_view entry, vk::ShaderStageFlagBits stage);


}

#endif