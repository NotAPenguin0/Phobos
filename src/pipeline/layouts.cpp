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

    constexpr size_t binding_cnt = 2;
    constexpr size_t vp_index = 0;
    constexpr size_t instance_index = 1;
    vk::DescriptorSetLayoutBinding bindings[2];
    bindings[vp_index].binding = 0;
    bindings[vp_index].descriptorType = vk::DescriptorType::eUniformBuffer;
    bindings[vp_index].descriptorCount = 1;
    bindings[vp_index].stageFlags = vk::ShaderStageFlagBits::eVertex;

    bindings[instance_index].binding = 1;
    bindings[instance_index].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[instance_index].descriptorCount = 1;
    bindings[instance_index].stageFlags = vk::ShaderStageFlagBits::eVertex;

    descriptor_set_layout_info.bindingCount = binding_cnt;
    descriptor_set_layout_info.pBindings = bindings;

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