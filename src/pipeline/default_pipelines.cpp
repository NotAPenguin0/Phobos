#include <phobos/pipeline/pipeline.hpp>
#include <phobos/pipeline/shader_info.hpp>

#include <phobos/renderer/meta.hpp>

#include <phobos/core/vulkan_context.hpp>
#include <phobos/util/shader_util.hpp>

#include <stl/literals.hpp>

namespace ph {
    
static void create_generic_pipeline(VulkanContext& ctx) {
    using namespace stl::literals;
      
    PipelineCreateInfo info;

    info.shaders.emplace_back(load_shader_code("data/shaders/generic.vert.spv"), "main", vk::ShaderStageFlagBits::eVertex);
    info.shaders.emplace_back(load_shader_code("data/shaders/generic.frag.spv"), "main", vk::ShaderStageFlagBits::eFragment);  

    // Fill in reflected information
    reflect_shaders(info);

    info.vertex_input_binding = vk::VertexInputBindingDescription(0, 11 * sizeof(float), vk::VertexInputRate::eVertex);
    info.vertex_attributes.emplace_back(0_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 0_u32);
    info.vertex_attributes.emplace_back(1_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 3 * (stl::uint32_t)sizeof(float));
    info.vertex_attributes.emplace_back(2_u32, 0_u32, vk::Format::eR32G32B32Sfloat, 6 * (stl::uint32_t)sizeof(float));
    info.vertex_attributes.emplace_back(3_u32, 0_u32, vk::Format::eR32G32Sfloat, 9 * (stl::uint32_t)sizeof(float));

    info.dynamic_states.push_back(vk::DynamicState::eViewport);
    info.dynamic_states.push_back(vk::DynamicState::eScissor);

    // Note that these are dynamic state so we don't need to fill in the fields
    info.viewports.emplace_back();
    info.scissors.emplace_back();
    
    // TODO: Make blending easier
    vk::PipelineColorBlendAttachmentState blend_attachment;
    blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    blend_attachment.blendEnable = false;
    info.blend_attachments.push_back(blend_attachment);

    ctx.pipelines.create_named_pipeline("generic", stl::move(info));
}

void create_default_pipelines(VulkanContext& ctx, PipelineManager& pipelines) {
    create_generic_pipeline(ctx);
}

}