#ifndef PHOBOS_PIPELINE_LAYOUT_HPP_
#define PHOBOS_PIPELINE_LAYOUT_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/pipeline/layouts.hpp>

#include <optional>
#include <unordered_map>

namespace ph {

class PipelineLayouts {
public:
    // Create a new layout to store in the database
    void create_layout(vk::Device device, PipelineLayoutID id, vk::PipelineLayoutCreateInfo const& info);

    // Throws an exception if the layout with requested ID was not found
    vk::PipelineLayout get_layout(PipelineLayoutID id);

    // Returns std::nullopt if the layout with requested ID was not found
    std::optional<vk::PipelineLayout> find_layout(PipelineLayoutID id) const;

    // Destroys the pipeline layout with specified ID
    void destroy(vk::Device device, PipelineLayoutID id);

    // Destroys all stored pipeline layouts.
    void destroy_all(vk::Device device);

private:
    std::unordered_map<uint32_t, vk::PipelineLayout> layouts;
};

}

#endif