#ifndef PHOBOS_PIPELINE_LAYOUTS_HPP_
#define PHOBOS_PIPELINE_LAYOUTS_HPP_

#include <vulkan/vulkan.hpp>
#include <phobos/forward.hpp>

namespace ph {

enum class PipelineLayoutID : uint32_t {
    eEmpty = 0,
    eDefault = 1,
    eCount
};

void create_pipeline_layouts(vk::Device device, PipelineLayouts& layouts);

}

#endif