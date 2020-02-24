#include <phobos/pipeline/layouts.hpp>
#include <phobos/pipeline/pipeline_layout.hpp>

#include <phobos/renderer/meta.hpp>

namespace ph {

static void create_empty_layout(vk::Device device, PipelineLayouts& layouts) {
    // A default constructed vk::PipelineLayoutCreateInfo has all the default values we need for an 
    // "empty" pipeline layout
    vk::PipelineLayoutCreateInfo info;
    layouts.create_layout(device, PipelineLayoutID::eEmpty, info);
}

static void create_default_layout(vk::Device device, PipelineLayouts& layouts) {
    vk::PipelineLayoutCreateInfo info;

    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info;

    constexpr size_t binding_cnt = 4;
    constexpr size_t camera_index = meta::bindings::generic::camera;
    constexpr size_t instance_index = meta::bindings::generic::instance_data;
    constexpr size_t material_index = meta::bindings::generic::textures;
    constexpr size_t light_index = meta::bindings::generic::lights;
    vk::DescriptorSetLayoutBinding bindings[binding_cnt];
    bindings[camera_index].binding = meta::bindings::generic::camera;
    bindings[camera_index].descriptorType = vk::DescriptorType::eUniformBuffer;
    bindings[camera_index].descriptorCount = 1;
    bindings[camera_index].stageFlags = vk::ShaderStageFlagBits::eVertex;

    bindings[instance_index].binding = meta::bindings::generic::instance_data;
    bindings[instance_index].descriptorType = vk::DescriptorType::eStorageBuffer;
    bindings[instance_index].descriptorCount = 1;
    bindings[instance_index].stageFlags = vk::ShaderStageFlagBits::eVertex;

    bindings[material_index].binding = meta::bindings::generic::textures;
    bindings[material_index].descriptorType = vk::DescriptorType::eCombinedImageSampler;
    // Upper limit for the amount of materials (for now)
    bindings[material_index].descriptorCount = 64;
    bindings[material_index].stageFlags = vk::ShaderStageFlagBits::eFragment;

    bindings[light_index].binding = meta::bindings::generic::lights;
    bindings[light_index].descriptorType = vk::DescriptorType::eUniformBuffer;
    bindings[light_index].descriptorCount = 1;
    bindings[light_index].stageFlags = vk::ShaderStageFlagBits::eFragment;

    descriptor_set_layout_info.bindingCount = binding_cnt;
    descriptor_set_layout_info.pBindings = bindings;

    vk::DescriptorSetLayoutBindingFlagsCreateInfo binding_flags;
    vk::DescriptorBindingFlags flags[binding_cnt];
    flags[camera_index] = static_cast<vk::DescriptorBindingFlags>(0);
    flags[instance_index] = static_cast<vk::DescriptorBindingFlags>(0);
    flags[light_index] = static_cast<vk::DescriptorBindingFlagBits>(0);
    flags[material_index] = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eVariableDescriptorCount;
    binding_flags.bindingCount = binding_cnt;
    binding_flags.pBindingFlags = flags;
    descriptor_set_layout_info.pNext = &binding_flags;

    vk::DescriptorSetLayout set_layout = device.createDescriptorSetLayout(descriptor_set_layout_info);
    info.setLayoutCount = 1;
    info.pSetLayouts = &set_layout;
    
    vk::PushConstantRange material_push_constant;
    material_push_constant.offset = 0;
    material_push_constant.size = sizeof(uint32_t);
    material_push_constant.stageFlags = vk::ShaderStageFlagBits::eFragment;

    info.pushConstantRangeCount = 1;
    info.pPushConstantRanges = &material_push_constant;

    layouts.create_layout(device, PipelineLayoutID::eDefault, info);
}

void create_pipeline_layouts(vk::Device device, PipelineLayouts& layouts) {
    create_empty_layout(device, layouts);
    create_default_layout(device, layouts);
}

}