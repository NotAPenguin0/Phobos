#include <phobos/pipeline/pipeline.hpp>

namespace ph {

vk::GraphicsPipelineCreateInfo PipelineCreateInfo::vk_info() const {
    vk::GraphicsPipelineCreateInfo info;

    info.stageCount = shaders.size();
    info.pStages = shaders.data();
    info.layout = layout;
    info.pVertexInputState = &vertex_input;
    info.pInputAssemblyState = &input_assembly;
    info.pViewportState = &viewport_state;
    info.pRasterizationState = &rasterizer;
    info.pMultisampleState = &multisample;
    info.pColorBlendState = &blending;
    info.pDepthStencilState = &depth_stencil;
    info.pDynamicState = &dynamic_state;
    info.renderPass = render_pass;
    info.subpass = subpass;

    return info;
}

void PipelineCreateInfo::finalize() {
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &vertex_input_binding;
    vertex_input.vertexAttributeDescriptionCount = vertex_attributes.size();
    vertex_input.pVertexAttributeDescriptions = vertex_attributes.data();

    blending.logicOpEnable = blend_logic_op_enable;
    blending.attachmentCount = blend_attachments.size();
    blending.pAttachments = blend_attachments.data();

    dynamic_state.dynamicStateCount = dynamic_states.size();
    dynamic_state.pDynamicStates = dynamic_states.data();

    viewport_state.viewportCount = viewports.size();
    viewport_state.pViewports = viewports.data();
    viewport_state.scissorCount = scissors.size();
    viewport_state.pScissors = scissors.data();
}

void PipelineManager::create_named_pipeline(std::string name, PipelineCreateInfo&& info) {
    pipeline_infos.emplace(stl::move(name), stl::move(info));
}

PipelineCreateInfo const* PipelineManager::get_named_pipeline(std::string const& name) const {
    auto it = pipeline_infos.find(name);
    if (it != pipeline_infos.end()) return &it->second;
    return nullptr;
}

void PipelineManager::destroy_all(vk::Device device) {
    for (auto [id, pipeline] : pipeline_infos) {
        for (auto const& shader : pipeline.shaders) {
            device.destroyShaderModule(shader.module);
        }
    }

    pipeline_infos.clear();
}

}