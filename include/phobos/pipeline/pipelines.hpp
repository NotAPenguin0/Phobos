#ifndef PHOBOS_PIPELINES_HPP_
#define PHOBOS_PIPELINES_HPP_

#include <vulkan/vulkan.hpp>
#include <phobos/forward.hpp>


namespace ph {

enum class PipelineID : uint32_t {
    eGeneric,
    eCount
};

void create_pipelines(VulkanContext& ctx, PipelineManager& pipelines);

}

#endif