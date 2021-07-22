#include <phobos/shader.hpp>

#include <string>
#include <fstream>

namespace ph {

ShaderMeta::Binding ShaderMeta::operator[](std::string_view name) const {
    return bindings.at(std::string(name));
}

void ShaderMeta::add_binding(std::string_view name, Binding info) {
    bindings[std::string(name)] = info;
}

std::vector<uint32_t> load_shader_code(std::string_view path) {
    std::ifstream file(path.data(), std::ios::binary);
    std::string code = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

    std::vector<uint32_t> spv(code.size() / 4);
    std::memcpy(spv.data(), &code[0], code.size());
    return spv;
}

}