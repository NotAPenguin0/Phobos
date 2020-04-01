#include <phobos/pipeline/pipelines.hpp>
#include <phobos/pipeline/pipeline.hpp>

#include <phobos/core/vulkan_context.hpp>
#include <phobos/util/shader_util.hpp>

#include <stl/literals.hpp>

namespace ph {

static void create_generic_pipeline2(VulkanContext& ctx) {
    using namespace stl::literals;

    PipelineCreateInfo info;
    info.layout = ctx.pipeline_layouts.get_layout(PipelineLayoutID::eDefault).handle;

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
    create_generic_pipeline2(ctx);
}

}