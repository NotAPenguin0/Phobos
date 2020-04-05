#include <phobos/pipeline/pipeline.hpp>

#include <phobos/renderer/meta.hpp>

#include <phobos/core/vulkan_context.hpp>
#include <phobos/util/shader_util.hpp>

#include <stl/literals.hpp>

namespace ph {

static void create_generic_pipeline(VulkanContext& ctx) {
    using namespace stl::literals;

    DescriptorSetLayoutCreateInfo set_layout_info;
    set_layout_info.bindings.resize(4);
    auto& camera_binding = set_layout_info.bindings[meta::bindings::generic::camera];
    auto& instance_binding = set_layout_info.bindings[meta::bindings::generic::transforms];
    auto& material_binding = set_layout_info.bindings[meta::bindings::generic::textures];
    auto& light_binding = set_layout_info.bindings[meta::bindings::generic::lights];

    camera_binding.binding = meta::bindings::generic::camera;
    camera_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    camera_binding.descriptorCount = 1;
    camera_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    instance_binding.binding = meta::bindings::generic::transforms;
    instance_binding.descriptorType = vk::DescriptorType::eStorageBuffer;
    instance_binding.descriptorCount = 1;
    instance_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    material_binding.binding = meta::bindings::generic::textures;
    material_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    // #Tag(TexLimit)
    material_binding.descriptorCount = meta::max_textures_bound;
    material_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    light_binding.binding = meta::bindings::generic::lights;
    light_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    light_binding.descriptorCount = 1;
    light_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    set_layout_info.flags.resize(4);
    // Needed for descriptor indexing
    set_layout_info.flags[meta::bindings::generic::textures] = 
        vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eVariableDescriptorCount;

    PipelineLayoutCreateInfo layout_info;
    layout_info.set_layout = stl::move(set_layout_info);
    vk::PushConstantRange indices_push_constants;
    indices_push_constants.offset = 0;
    indices_push_constants.size = 2 * sizeof(uint32_t);
    indices_push_constants.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    layout_info.push_constants.push_back(indices_push_constants);

    PipelineCreateInfo info;
    info.layout = layout_info;
    
    info.vertex_input_binding = vk::VertexInputBindingDescription(0, 8 * sizeof(float), vk::VertexInputRate::eVertex);
    info.vertex_attributes.emplace_back(0_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 0_u32);
    info.vertex_attributes.emplace_back(1_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 3 * (stl::uint32_t)sizeof(float));
    info.vertex_attributes.emplace_back(2_u32, 0_u32, vk::Format::eR32G32Sfloat, 6 * (stl::uint32_t)sizeof(float));

    vk::ShaderModule vertex_shader = load_shader(ctx.device, "data/shaders/generic.vert.spv");
    vk::ShaderModule fragment_shader = load_shader(ctx.device, "data/shaders/generic.frag.spv");

    info.shaders.emplace_back(vk::PipelineShaderStageCreateFlags{}, vk::ShaderStageFlagBits::eVertex, vertex_shader, "main");
    info.shaders.emplace_back(vk::PipelineShaderStageCreateFlags{}, vk::ShaderStageFlagBits::eFragment, fragment_shader, "main");   

    info.input_assembly.primitiveRestartEnable = false;
    info.input_assembly.topology = vk::PrimitiveTopology::eTriangleList;

    info.depth_stencil.depthTestEnable = true;
    info.depth_stencil.depthWriteEnable = true;
    info.depth_stencil.depthCompareOp = vk::CompareOp::eLess;
    info.depth_stencil.depthBoundsTestEnable = false;
    info.depth_stencil.stencilTestEnable = false;

    info.dynamic_states.push_back(vk::DynamicState::eViewport);
    info.dynamic_states.push_back(vk::DynamicState::eScissor);

    // Note that these are dynamic state so we don't need to fill in the fields
    info.viewports.emplace_back();
    info.scissors.emplace_back();

    info.rasterizer.depthClampEnable = false;
    info.rasterizer.polygonMode = vk::PolygonMode::eFill;
    info.rasterizer.lineWidth = 1.0f;
    info.rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    info.rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
    info.rasterizer.rasterizerDiscardEnable = false;
    info.rasterizer.depthBiasEnable = false;

    info.multisample.sampleShadingEnable = false;
    info.multisample.rasterizationSamples = vk::SampleCountFlagBits::e1;

    info.blend_logic_op_enable = false;
    vk::PipelineColorBlendAttachmentState blend_attachment;
    blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend_attachment.blendEnable = false;
    info.blend_attachments.push_back(blend_attachment);
    info.finalize();

    ctx.pipelines.create_named_pipeline("generic", stl::move(info));
}

void create_pipelines(VulkanContext& ctx, PipelineManager& pipelines) {
    create_generic_pipeline(ctx);
}

}