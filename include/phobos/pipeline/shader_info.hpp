#ifndef PHOBOS_SHADER_INFO_HPP_
#define PHOBOS_SHADER_INFO_HPP_

#include <string_view>
#include <unordered_map>

#include <vulkan/vulkan.hpp>
#include <phobos/forward.hpp>

namespace ph {

struct PipelineCreateInfo;
struct ComputePipelineCreateInfo;

class ShaderInfo {
public:
    struct BindingInfo {
        uint32_t binding;
        vk::DescriptorType type;
    };

    BindingInfo operator[](std::string_view name) const;

    void add_binding(std::string_view name, BindingInfo info);
private:
    std::unordered_map<std::string, BindingInfo> bindings;
};

// Provides meta info like descriptor bindings on shaders in the pipeline, and updates members of the pipeline 
// create info to match these shaders (Pipeline layout and ShaderInfo field).
void reflect_shaders(VulkanContext& ctx, ph::PipelineCreateInfo& pci);
void reflect_shaders(VulkanContext& ctx, ph::ComputePipelineCreateInfo& pci);

}

#endif