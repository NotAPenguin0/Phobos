#include <phobos/util/shader_util.hpp>

#include <fstream>
#include <vector>
#include <string>

namespace ph {

std::vector<uint32_t> load_shader_code(std::string_view file_path) {
    std::ifstream file(file_path.data(), std::ios::binary);
    std::string code = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    
    std::vector<uint32_t> spv(code.size() / 4);
    std::memcpy(spv.data(), &code[0], code.size());
    return spv;
}

}