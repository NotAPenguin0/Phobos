#ifndef PHOBOS_PIPELINE_HPP_
#define PHOBOS_PIPELINE_HPP_

#include <vulkan/vulkan.hpp>
#include <optional>
#include <unordered_map>
#include <string>
#include <mutex>

#include <stl/span.hpp>
#include <stl/vector.hpp>

#include <phobos/forward.hpp>
#include <phobos/pipeline/shader_info.hpp>
#include <phobos/util/image_util.hpp>
#include <phobos/util/buffer_util.hpp>
#include <phobos/util/shader_util.hpp>

namespace ph {

struct RenderPass;

struct DescriptorSetLayoutCreateInfo {
    stl::vector<vk::DescriptorSetLayoutBinding> bindings;
    // Optional. Leave empty to use default flags
    stl::vector<vk::DescriptorBindingFlags> flags;
};

// TODO: Custom BufferView?
struct DescriptorBufferInfo {
    vk::Buffer buffer = nullptr;
    vk::DeviceSize offset = 0;
    vk::DeviceSize range = 0;
};

struct DescriptorImageInfo {
    vk::Sampler sampler = nullptr;
    ImageView view{};
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
};

struct DescriptorBinding {
    stl::size_t binding = 0;
    vk::DescriptorType type;

    union DescriptorContents {
        DescriptorBufferInfo buffer;
        DescriptorImageInfo image;
    };

    stl::vector<DescriptorContents> descriptors;
};

DescriptorBinding make_image_descriptor(uint32_t binding, ImageView view, vk::Sampler sampler, vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
DescriptorBinding make_image_descriptor_array(uint32_t binding, stl::span<ImageView> views, vk::Sampler sampler,
    vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
DescriptorBinding make_buffer_descriptor(uint32_t binding, vk::DescriptorType type, vk::Buffer buffer, vk::DeviceSize size, vk::DeviceSize offset = 0);

DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, ImageView view, vk::Sampler sampler, 
        vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, ImageView view, vk::ImageLayout layout);
DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, stl::span<ImageView> views, vk::Sampler sampler, 
        vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal);
DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, vk::Buffer buffer, vk::DeviceSize size, vk::DeviceSize offset = 0);
DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, RawBuffer& buffer, vk::DeviceSize offset = 0);

DescriptorBinding make_descriptor(ShaderInfo::BindingInfo binding, BufferSlice slice);

struct DescriptorSetBinding {
    void add(DescriptorBinding binding) { bindings.push_back(stl::move(binding)); }

    stl::vector<DescriptorBinding> bindings;
    // Leave this on nullptr to use default pool
    vk::DescriptorPool pool = nullptr;
private:
    friend class CommandBuffer;
    friend struct std::hash<DescriptorSetBinding>;
    DescriptorSetLayoutCreateInfo set_layout;
};

struct PipelineLayoutCreateInfo {
    DescriptorSetLayoutCreateInfo set_layout;
    stl::vector<vk::PushConstantRange> push_constants;
};

struct PipelineLayout {
    vk::PipelineLayout  layout;
    vk::DescriptorSetLayout set_layout;
};

enum class PipelineType {
    Graphics,
    Compute
};

struct Pipeline {
    vk::Pipeline pipeline;
    PipelineLayout layout;
    PipelineType type;

    std::string name;
};

struct ShaderModuleCreateInfo {
    std::vector<uint32_t> code;
    std::string entry_point;
    vk::ShaderStageFlagBits stage;

    // Set by reflection function, or manually
    size_t code_hash = 0;
};

struct PipelineCreateInfo {
    // debug name for this pipeline, defaults to the name given in the pipeline manager
    std::string debug_name;
    // Contains reflected information with binding names, etc
    ShaderInfo shader_info;

    PipelineLayoutCreateInfo layout;

    std::vector<vk::VertexInputBindingDescription> vertex_input_bindings;
    stl::vector<vk::VertexInputAttributeDescription> vertex_attributes;
    stl::vector<ShaderHandle> shaders;
    vk::PipelineInputAssemblyStateCreateInfo input_assembly =
        vk::PipelineInputAssemblyStateCreateInfo {
            vk::PipelineInputAssemblyStateCreateFlags{},
            vk::PrimitiveTopology::eTriangleList,
            false
        };
    vk::PipelineDepthStencilStateCreateInfo depth_stencil = 
        vk::PipelineDepthStencilStateCreateInfo {
            vk::PipelineDepthStencilStateCreateFlags{},
            true, true, vk::CompareOp::eLess, false, false
        };
    stl::vector<vk::DynamicState> dynamic_states;
    vk::PipelineRasterizationStateCreateInfo rasterizer = 
        vk::PipelineRasterizationStateCreateInfo {
            vk::PipelineRasterizationStateCreateFlags{},
            false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack,
            vk::FrontFace::eCounterClockwise, false, 0.0f, 0.0f, 0.0f, 1.0f
        };
    vk::PipelineMultisampleStateCreateInfo multisample = 
        vk::PipelineMultisampleStateCreateInfo {
            vk::PipelineMultisampleStateCreateFlags{},
            vk::SampleCountFlagBits::e1
        };
    stl::vector<vk::PipelineColorBlendAttachmentState> blend_attachments;
    stl::vector<vk::Viewport> viewports;
    stl::vector<vk::Rect2D> scissors;
    bool blend_logic_op_enable = false;
    vk::GraphicsPipelineCreateInfo vk_info() const;
    
    void finalize();
private:
    friend class Renderer;
    friend struct std::hash<PipelineCreateInfo>;
    friend class RenderGraph;
    friend Pipeline create_or_get_pipeline(VulkanContext*, PerThreadContext*, RenderPass*, PipelineCreateInfo);

    vk::PipelineVertexInputStateCreateInfo vertex_input;
    vk::PipelineColorBlendStateCreateInfo blending;
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    vk::PipelineViewportStateCreateInfo viewport_state;

    // These are set before creating the actual VkPipeline
    vk::RenderPass render_pass;
    PipelineLayout pipeline_layout;
    stl::uint32_t subpass;
};

struct ComputePipelineCreateInfo {
    std::string debug_name;
    ShaderInfo shader_info;
    ShaderHandle shader;
    PipelineLayoutCreateInfo layout;

    vk::ComputePipelineCreateInfo vk_info() const;
    void finalize();

private:
    friend class Renderer;
    friend struct std::hash<ComputePipelineCreateInfo>;
    friend class RenderGraph;
    friend Pipeline create_or_get_compute_pipeline(VulkanContext*, PerThreadContext*, ComputePipelineCreateInfo);

    PipelineLayout pipeline_layout;
};

Pipeline create_or_get_pipeline(VulkanContext* ctx, PerThreadContext* ptc, RenderPass* pass, PipelineCreateInfo pci);
Pipeline create_or_get_compute_pipeline(VulkanContext* ctx, PerThreadContext* ptc, ComputePipelineCreateInfo pci);

class PipelineManager {
public:
    void create_named_pipeline(std::string name, PipelineCreateInfo&& info);
    void create_named_pipeline(std::string name, ComputePipelineCreateInfo&& info);
    PipelineCreateInfo const* get_named_pipeline(std::string const& name) const;
    ComputePipelineCreateInfo const* get_named_compute_pipeline(std::string const& name) const;
private:
    mutable std::mutex mutex;
    mutable std::mutex compute_mutex;
    std::unordered_map<std::string, PipelineCreateInfo> pipeline_infos;
    std::unordered_map<std::string, ComputePipelineCreateInfo> compute_pipelines;
};

}

#endif