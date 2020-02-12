#ifndef PHOBOS_PIPELINE_HPP_
#define PHOBOS_PIPELINE_HPP_

#include <vulkan/vulkan.hpp>
#include <optional>
#include <unordered_map>

#include <phobos/pipeline/pipelines.hpp>


namespace ph {


class PipelineManager {
public:
    void create_pipeline(vk::Device device, PipelineID id, vk::GraphicsPipelineCreateInfo const& info);

    // Throws an exception if the pipeline with requested ID was not found
    vk::Pipeline get_pipeline(PipelineID id);

    // Returns std::nullopt if the pipeline with requested ID was not found
    std::optional<vk::Pipeline> find_pipeline(PipelineID id) const;

    // Destroys the pipeline with specified ID
    void destroy(vk::Device device, PipelineID id);

    // Destroys all stored pipeline.
    void destroy_all(vk::Device device);

private:
    std::unordered_map<uint32_t, vk::Pipeline> pipelines;

};

}

#endif