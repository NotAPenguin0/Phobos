#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <phobos/shader.hpp>
#include <phobos/image.hpp>
#include <phobos/buffer.hpp>

namespace ph {

namespace impl {
    class CacheImpl;
}

class Context;

enum class PipelineStage {
    VertexShader = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    FragmentShader = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    ComputeShader = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    AttachmentOutput = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
};

enum class PipelineType {
    Graphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
    Compute = VK_PIPELINE_BIND_POINT_COMPUTE
};

struct DescriptorBufferInfo {
    VkBuffer buffer = nullptr;
    VkDeviceSize offset = 0;
    VkDeviceSize range = 0;
};

struct DescriptorImageInfo {
    VkSampler sampler = nullptr;
    ImageView view{};
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
};

struct DescriptorBinding {
    uint32_t binding = 0;
    VkDescriptorType type;

    struct DescriptorContents {
        DescriptorBufferInfo buffer = {};
        DescriptorImageInfo image;
    };

    std::vector<DescriptorContents> descriptors;
};

struct DescriptorSetLayoutCreateInfo {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    // Optional. Leave empty to use default flags
    std::vector<VkDescriptorBindingFlags> flags;
};

struct DescriptorSetBinding {
    std::vector<DescriptorBinding> bindings;
    VkDescriptorPool pool = nullptr;
private:
    friend class impl::CacheImpl;
    friend struct std::hash<DescriptorSetBinding>;
    VkDescriptorSetLayout set_layout = nullptr;
};

struct PipelineLayoutCreateInfo {
    DescriptorSetLayoutCreateInfo set_layout;
    std::vector<VkPushConstantRange> push_constants;
};

struct PipelineLayout {
    VkPipelineLayout handle = nullptr;
    VkDescriptorSetLayout set_layout = nullptr;
};

struct Pipeline {
    VkPipeline handle = nullptr;
    PipelineLayout layout;
    PipelineType type;

    std::string name;
};

struct ShaderModuleCreateInfo {
    std::vector<uint32_t> code;
    std::string entry_point;
    PipelineStage stage;
};

struct PipelineCreateInfo {
    std::string name{};
    PipelineLayoutCreateInfo layout;

    ShaderMeta meta{};

    std::vector<VkVertexInputBindingDescription> vertex_input_bindings;
    std::vector<VkVertexInputAttributeDescription> vertex_attributes;
    std::vector<ShaderHandle> shaders;
    VkPipelineInputAssemblyStateCreateInfo input_assembly =
        VkPipelineInputAssemblyStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VkPipelineInputAssemblyStateCreateFlags{},
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = false
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil =
        VkPipelineDepthStencilStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VkPipelineDepthStencilStateCreateFlags{},
            .depthTestEnable = true, 
            .depthWriteEnable = true, 
            .depthCompareOp = VK_COMPARE_OP_LESS, 
            .depthBoundsTestEnable = false, 
            .stencilTestEnable = false,
            .front = VkStencilOp{},
            .back = VkStencilOp{},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 0.0f
    };

    std::vector<VkDynamicState> dynamic_states;
    VkPipelineRasterizationStateCreateInfo rasterizer =
        VkPipelineRasterizationStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VkPipelineRasterizationStateCreateFlags{},
            .depthClampEnable = false, 
            .rasterizerDiscardEnable = false, 
            .polygonMode = VK_POLYGON_MODE_FILL, 
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE, 
            .depthBiasEnable = false, 
            .depthBiasConstantFactor = 0.0f, 
            .depthBiasClamp = 0.0f, 
            .depthBiasSlopeFactor = 0.0f, 
            .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisample =
        VkPipelineMultisampleStateCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VkPipelineMultisampleStateCreateFlags{},
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = false,
            .minSampleShading = 0.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = false,
            .alphaToOneEnable = false
    };

    std::vector<VkPipelineColorBlendAttachmentState> blend_attachments;
    std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors;
    bool blend_logic_op_enable = false;
};

class DescriptorBuilder {
public:
    static DescriptorBuilder create(Context& ctx, Pipeline const& pipeline);

    DescriptorBuilder& add_sampled_image(uint32_t binding, ImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DescriptorBuilder& add_sampled_image(ShaderMeta::Binding const& binding, ImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DescriptorBuilder& add_sampled_image(std::string_view binding, ImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    DescriptorBuilder& add_uniform_buffer(uint32_t binding, BufferSlice buffer);
    DescriptorBuilder& add_uniform_buffer(ShaderMeta::Binding const& binding, BufferSlice buffer);
    DescriptorBuilder& add_uniform_buffer(std::string_view binding, BufferSlice buffer);

    DescriptorBuilder& add_pNext(void* p);

    VkDescriptorSet get();
private:
    Context* ctx = nullptr;
    Pipeline pipeline{};
    DescriptorSetBinding info{};
    std::vector<void*> pNext_chain{};
};

class PipelineBuilder {
public:
    static PipelineBuilder create(Context& ctx, std::string_view name);

    // Note that vertex attributes must be added in order. add_vertex_input() must be called before adding any attributes
    PipelineBuilder& add_vertex_input(uint32_t binding, VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX);
    PipelineBuilder& add_vertex_attribute(uint32_t binding, uint32_t location, VkFormat format);
    PipelineBuilder& add_shader(std::string_view path, std::string_view entry, PipelineStage stage);
    PipelineBuilder& add_shader(ShaderHandle shader);
    PipelineBuilder& set_depth_test(bool test);
    PipelineBuilder& set_depth_write(bool write);
    PipelineBuilder& set_depth_op(VkCompareOp op);
    PipelineBuilder& add_dynamic_state(VkDynamicState state);
    PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
    PipelineBuilder& set_cull_mode(VkCullModeFlags mode);
    PipelineBuilder& set_front_face(VkFrontFace face);
    PipelineBuilder& set_samples(VkSampleCountFlagBits samples);
    PipelineBuilder& add_blend_attachment(bool enable = false,
        VkBlendFactor src_color_factor = VK_BLEND_FACTOR_ONE, VkBlendFactor dst_color_factor = VK_BLEND_FACTOR_ONE, VkBlendOp color_op = VK_BLEND_OP_ADD,
        VkBlendFactor src_alpha_factor = VK_BLEND_FACTOR_ONE, VkBlendFactor dst_alpha_factor = VK_BLEND_FACTOR_ONE, VkBlendOp alpha_op = VK_BLEND_OP_ADD,
        VkColorComponentFlags write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);
    PipelineBuilder& add_viewport(VkViewport vp);
    PipelineBuilder& add_scissor(VkRect2D scissor);
    PipelineBuilder& enable_blend_op(bool enable);
    PipelineBuilder& reflect();

    ph::PipelineCreateInfo get();
private:
    Context* ctx = nullptr;
    ph::PipelineCreateInfo pci{};

    std::unordered_map<uint32_t /* vertex input binding*/, uint32_t /* current byte offset*/> vertex_binding_offsets;
};

}