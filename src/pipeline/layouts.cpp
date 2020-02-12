#include <phobos/pipeline/layouts.hpp>
#include <phobos/pipeline/pipeline_layout.hpp>

namespace ph {

static void create_empty_layout(vk::Device device, PipelineLayouts& layouts) {
    // A default constructed vk::PipelineLayoutCreateInfo has all the default values we need for an 
    // "empty" pipeline layout
    vk::PipelineLayoutCreateInfo info;
    layouts.create_layout(device, PipelineLayoutID::eEmpty, info);
}

void create_pipeline_layouts(vk::Device device, PipelineLayouts& layouts) {
    create_empty_layout(device, layouts);
}

}