#ifndef PHOBOS_PIPELINE_HPP_
#define PHOBOS_PIPELINE_HPP_

#include <vulkan/vulkan.hpp>
#include <optional>
#include <unordered_map>
#include <string>

#include <stl/vector.hpp>

#include <phobos/pipeline/pipelines.hpp>


namespace ph {

struct PipelineCreateInfo {
    vk::PipelineLayout layout;

    vk::VertexInputBindingDescription vertex_input_binding;
    stl::vector<vk::VertexInputAttributeDescription> vertex_attributes;
    stl::vector<vk::PipelineShaderStageCreateInfo> shaders;
    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    vk::PipelineDepthStencilStateCreateInfo depth_stencil;
    stl::vector<vk::DynamicState> dynamic_states;
    vk::PipelineRasterizationStateCreateInfo rasterizer;
    vk::PipelineMultisampleStateCreateInfo multisample;
    stl::vector<vk::PipelineColorBlendAttachmentState> blend_attachments;
    stl::vector<vk::Viewport> viewports;
    stl::vector<vk::Rect2D> scissors;
    bool blend_logic_op_enable;
    vk::GraphicsPipelineCreateInfo vk_info() const;
    
    void finalize();
private:
    friend class Renderer;
    friend struct std::hash<PipelineCreateInfo>;
    friend class RenderGraph;

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vk::PipelineColorBlendStateCreateInfo blending;
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    vk::PipelineViewportStateCreateInfo viewport_state;

    vk::RenderPass render_pass;
    stl::uint32_t subpass;
};

class PipelineManager {
public:
    void create_named_pipeline(std::string name, PipelineCreateInfo&& info);
    PipelineCreateInfo const* get_named_pipeline(std::string const& name) const;

    // Destroys all stored pipelines.
    void destroy_all(vk::Device device);

private:
    std::unordered_map<std::string, PipelineCreateInfo> pipeline_infos;
};

}

#endif