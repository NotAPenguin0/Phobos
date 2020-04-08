#ifndef PHOBOS_SHADER_UTIL_HPP_
#define PHOBOS_SHADER_UTIL_HPP_

#include <string_view>
#include <vector>

namespace ph {


std::vector<uint32_t> load_shader_code(std::string_view file_path);


}

#endif