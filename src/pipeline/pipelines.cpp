#include <phobos/pipeline/pipelines.hpp>
#include <phobos/pipeline/pipeline.hpp>

#include <phobos/core/vulkan_context.hpp>
#include <phobos/util/shader_util.hpp>

namespace ph {

namespace generic_pipeline {

static vk::VertexInputBindingDescription const& vertex_input_binding() {
    static vk::VertexInputBindingDescription binding(0, 5 * sizeof(float), vk::VertexInputRate::eVertex);
    return binding;
}

static std::array<vk::VertexInputAttributeDescription, 2> const& vertex_input_attributes() {
    static std::array<vk::VertexInputAttributeDescription, 2> attributes {{
        // Position
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),
        // TexCoords
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, 3 * sizeof(float))
    }};
    return attributes;
}

// The generic pipeline expects vertices in the format
// vec3 Position        vec2 TexCoords
static vk::PipelineVertexInputStateCreateInfo vertex_input() {
    vk::PipelineVertexInputStateCreateInfo info;
    info.vertexBindingDescriptionCount = 1;
    // Taking these addresses is safe since it's all static data
    info.pVertexBindingDescriptions = &vertex_input_binding();
    info.vertexAttributeDescriptionCount = vertex_input_attributes().size();
    info.pVertexAttributeDescriptions = vertex_input_attributes().data();
    

    return info;
}

static std::array<vk::PipelineShaderStageCreateInfo, 2> shaders(VulkanContext& ctx) {
    vk::ShaderModule vertex_shader = load_shader(ctx.device, "data/shaders/generic.vert.spv");
    vk::ShaderModule fragment_shader = load_shader(ctx.device, "data/shaders/generic.frag.spv");

    std::array<vk::PipelineShaderStageCreateInfo, 2> info;
    info[0].module = vertex_shader;
    info[0].pName = "main";
    info[0].stage = vk::ShaderStageFlagBits::eVertex;
    
    info[1].module = fragment_shader;
    info[1].pName = "main";
    info[1].stage = vk::ShaderStageFlagBits::eFragment;

    return info;
}

static vk::PipelineInputAssemblyStateCreateInfo input_assembly() {
    vk::PipelineInputAssemblyStateCreateInfo info;
    info.primitiveRestartEnable = false;
    info.topology = vk::PrimitiveTopology::eTriangleList;
    return info;
}

static vk::Viewport viewport(VulkanContext& ctx) {
    vk::Viewport vp;
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.width = ctx.swapchain.extent.width;
    vp.height = ctx.swapchain.extent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    return vp;
}

static vk::Rect2D scissor(VulkanContext& ctx) {
    vk::Rect2D s;
    s.offset = vk::Offset2D{0, 0};
    s.extent = ctx.swapchain.extent;
    return s;
}

static vk::PipelineRasterizationStateCreateInfo rasterizer() {
    vk::PipelineRasterizationStateCreateInfo info;
    info.depthClampEnable = false;
    info.polygonMode = vk::PolygonMode::eFill;
    info.lineWidth = 1.0f;
    info.cullMode = vk::CullModeFlagBits::eBack;
    info.frontFace = vk::FrontFace::eCounterClockwise;
    info.rasterizerDiscardEnable = false;
    info.depthBiasEnable = false;

    return info;
}

static vk::PipelineMultisampleStateCreateInfo multisample() {
    vk::PipelineMultisampleStateCreateInfo info;
    // Do not enable multisampling for now
    info.sampleShadingEnable = false;
    info.rasterizationSamples = vk::SampleCountFlagBits::e1;
    return info;
}

static vk::PipelineColorBlendStateCreateInfo color_blending() {
    // static so this variable doesn't go out of scope
    static vk::PipelineColorBlendAttachmentState attachment;
    attachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    // Alpha blending is disabled for this pipeline
    attachment.blendEnable = false;

    vk::PipelineColorBlendStateCreateInfo info;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.logicOpEnable = false;

    return info;
}

} // namespace generic_pipeline

static void create_generic_pipeline(VulkanContext& ctx, PipelineManager& pipelines) {
    vk::GraphicsPipelineCreateInfo info;

    // Get all required components for this grahics pipeline
    vk::PipelineVertexInputStateCreateInfo vertex_input_info = generic_pipeline::vertex_input();
    std::array<vk::PipelineShaderStageCreateInfo, 2> shader_info = generic_pipeline::shaders(ctx);
    vk::PipelineInputAssemblyStateCreateInfo input_assembly = generic_pipeline::input_assembly();
    vk::Viewport viewport = generic_pipeline::viewport(ctx);
    vk::Rect2D scissor = generic_pipeline::scissor(ctx);

    vk::PipelineViewportStateCreateInfo viewport_info;
    viewport_info.viewportCount = 1;
    viewport_info.pViewports = &viewport;
    viewport_info.scissorCount = 1;
    viewport_info.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizer = generic_pipeline::rasterizer();
    vk::PipelineMultisampleStateCreateInfo multisample_info = generic_pipeline::multisample();
    vk::PipelineColorBlendStateCreateInfo blending = generic_pipeline::color_blending();

    info.stageCount = shader_info.size();
    info.pStages = shader_info.data();
    info.layout = ctx.pipeline_layouts.get_layout(PipelineLayoutID::eMVPOnly).handle;
    info.pVertexInputState = &vertex_input_info;
    info.pInputAssemblyState = &input_assembly;
    info.pViewportState = &viewport_info;
    info.pRasterizationState = &rasterizer;
    info.pMultisampleState = &multisample_info;
    info.pColorBlendState = &blending;

    info.renderPass = ctx.default_render_pass;
    info.subpass = 0;
    
    pipelines.create_pipeline(ctx.device, PipelineID::eGeneric, info);

    // We don't need the shader modules anymore
    ctx.device.destroyShaderModule(shader_info[0].module);
    ctx.device.destroyShaderModule(shader_info[1].module);
}

void create_pipelines(VulkanContext& ctx, PipelineManager& pipelines) {
    create_generic_pipeline(ctx, pipelines);
}

}