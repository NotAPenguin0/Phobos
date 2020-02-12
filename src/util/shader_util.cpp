#include <phobos/util/shader_util.hpp>

#include <fstream>
#include <vector>

namespace ph {

vk::ShaderModule load_shader(vk::Device device, std::string_view file_path) {
    std::ifstream file(file_path.data(), std::ios::binary);
    std::vector<char> code = std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

    vk::ShaderModuleCreateInfo info;
    info.codeSize = code.size();
    info.pCode = reinterpret_cast<uint32_t const*>(code.data());
    return device.createShaderModule(info);
}

}