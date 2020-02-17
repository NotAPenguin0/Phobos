#ifndef PHOBOS_PIPELINE_LAYOUT_HPP_
#define PHOBOS_PIPELINE_LAYOUT_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/pipeline/layouts.hpp>

#include <optional>
#include <unordered_map>

namespace ph {

struct PipelineLayout {
    vk::PipelineLayout handle;
    vk::DescriptorSetLayout descriptor_set_layout = nullptr;
};

class PipelineLayouts {
public:
    // Create a new layout to store in the database. Takes ownership of any eventual VkDescriptorSetLayouts in the info struct
    void create_layout(vk::Device device, PipelineLayoutID id, vk::PipelineLayoutCreateInfo const& info);

    // Throws an exception if the layout with requested ID was not found
    PipelineLayout get_layout(PipelineLayoutID id);

    // Returns std::nullopt if the layout with requested ID was not found
    std::optional<PipelineLayout> find_layout(PipelineLayoutID id) const;

    // Destroys the pipeline layout with specified ID
    void destroy(vk::Device device, PipelineLayoutID id);

    // Destroys all stored pipeline layouts.
    void destroy_all(vk::Device device);

private:
    std::unordered_map<uint32_t, PipelineLayout> layouts;
};

}

#endif