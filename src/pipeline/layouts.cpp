#include <phobos/pipeline/layouts.hpp>
#include <phobos/pipeline/pipeline_layout.hpp>

namespace ph {

static void create_empty_layout(vk::Device device, PipelineLayouts& layouts) {
    // A default constructed vk::PipelineLayoutCreateInfo has all the default values we need for an 
    // "empty" pipeline layout
    vk::PipelineLayoutCreateInfo info;
    layouts.create_layout(device, PipelineLayoutID::eEmpty, info);
}

static void create_mvp_only_layout(vk::Device device, PipelineLayouts& layouts) {
    vk::PipelineLayoutCreateInfo info;

    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info;

    vk::DescriptorSetLayoutBinding binding;
    binding.binding = 0;
    binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    binding.descriptorCount = 1;
    binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    descriptor_set_layout_info.bindingCount = 1;
    descriptor_set_layout_info.pBindings = &binding;

    vk::DescriptorSetLayout set_layout = device.createDescriptorSetLayout(descriptor_set_layout_info);
    info.setLayoutCount = 1;
    info.pSetLayouts = &set_layout;

    layouts.create_layout(device, PipelineLayoutID::eMVPOnly, info);
}

void create_pipeline_layouts(vk::Device device, PipelineLayouts& layouts) {
    create_empty_layout(device, layouts);
    create_mvp_only_layout(device, layouts);
}

}