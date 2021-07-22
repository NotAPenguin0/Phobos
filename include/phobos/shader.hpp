#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <string>
#include <unordered_map>

#include <vulkan/vulkan.h>

namespace ph {

class ShaderMeta {
public:
    struct Binding {
        uint32_t binding;
        VkDescriptorType type;
    };

    Binding operator[](std::string_view name) const;

    void add_binding(std::string_view name, Binding info);
private:
    std::unordered_map<std::string, Binding> bindings;
};


struct ShaderHandle {
	static constexpr uint32_t none = static_cast<uint32_t>(-1);

	uint32_t id = none;
};

std::vector<uint32_t> load_shader_code(std::string_view file_path);

}