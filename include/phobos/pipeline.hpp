#pragma once

#include <string>
#include <vector>
#include <array>
#include <optional>
#include <span>

#include <vulkan/vulkan.h>

#include <phobos/shader.hpp>
#include <phobos/image.hpp>
#include <phobos/buffer.hpp>

namespace ph {

namespace impl {
    class CacheImpl;
}

#if PHOBOS_ENABLE_RAY_TRACING
struct AccelerationStructure;
#endif

class Context;

enum class PipelineStage {
    AllCommands = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    TopOfPipe = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    Transfer = VK_PIPELINE_STAGE_TRANSFER_BIT,
    VertexShader = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
    FragmentShader = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    ComputeShader = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    AttachmentOutput = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

    // These flags are useful to barrier depth writes.
    EarlyFragmentTests = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    LateFragmentTests = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
#if PHOBOS_ENABLE_RAY_TRACING
    AccelerationStructureBuild = VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
    RayTracingShader = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
#endif
    BottomOfPipe = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
};

enum class ShaderStage {
    Vertex = VK_SHADER_STAGE_VERTEX_BIT,
    Fragment = VK_SHADER_STAGE_FRAGMENT_BIT,
    Compute = VK_SHADER_STAGE_COMPUTE_BIT,
#if PHOBOS_ENABLE_RAY_TRACING
    RayGeneration = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
    ClosestHit = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
    RayMiss = VK_SHADER_STAGE_MISS_BIT_KHR,
    AnyHit = VK_SHADER_STAGE_ANY_HIT_BIT_KHR
#endif
};

enum class PipelineType {
    Graphics = VK_PIPELINE_BIND_POINT_GRAPHICS,
    Compute = VK_PIPELINE_BIND_POINT_COMPUTE,
#if PHOBOS_ENABLE_RAY_TRACING
    RayTracing = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR
#endif
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

#if PHOBOS_ENABLE_RAY_TRACING
struct DescriptorAccelerationStructureInfo {
    VkAccelerationStructureKHR structure = nullptr;
};
#endif

struct DescriptorBinding {
    DescriptorBinding() = default;
    DescriptorBinding(DescriptorBinding const&) = default;
    DescriptorBinding(DescriptorBinding&&) = default;

    DescriptorBinding& operator=(DescriptorBinding const&) = default;
    DescriptorBinding& operator=(DescriptorBinding&&) = default;

    uint32_t binding = 0;
    VkDescriptorType type{};

    struct DescriptorContents {
        DescriptorBufferInfo buffer{};
        DescriptorImageInfo image{};
#if PHOBOS_ENABLE_RAY_TRACING
        DescriptorAccelerationStructureInfo accel_structure{};
#endif
    };

    std::vector<DescriptorContents> descriptors;
};

struct DescriptorSetLayoutCreateInfo {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    // Optional. Leave empty to use default flags
    std::vector<VkDescriptorBindingFlags> flags;
};

struct DescriptorSetBinding {
    DescriptorSetBinding() = default;
    DescriptorSetBinding(DescriptorSetBinding const&) = default;

    DescriptorSetBinding& operator=(DescriptorSetBinding const&) = default;

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
    ShaderStage stage;
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
            .front = VkStencilOpState{},
            .back = VkStencilOpState{},
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

struct ComputePipelineCreateInfo {
    std::string name{};
    PipelineLayoutCreateInfo layout{};
    ShaderHandle shader;

    ShaderMeta meta{};

};

#if PHOBOS_ENABLE_RAY_TRACING

// These enum values are chosen so the shader groups can be sorted into arrays
enum class RayTracingShaderGroupType {
    RayGeneration = 0,
    RayMiss = 1,
    RayHit = 2,
    Callable = 3
};

struct RayTracingShaderGroup {
    RayTracingShaderGroupType type{};
    ShaderHandle general{ ShaderHandle::none };
    ShaderHandle closest_hit{ ShaderHandle::none };
    ShaderHandle any_hit{ ShaderHandle::none };
    ShaderHandle intersection{ ShaderHandle::none };
};

struct RayTracingPipelineCreateInfo {
    std::string name{};
    PipelineLayoutCreateInfo layout{};

    std::vector<ShaderHandle> shaders{};
    std::vector<RayTracingShaderGroup> shader_groups{};

    uint32_t max_recursion_depth = 1;

    ShaderMeta meta{};
};

struct ShaderBindingTable {
    ph::RawBuffer buffer;
    // Offset of the ray generation groups in elemetns
    uint32_t raygen_offset = 0;
    // Amount of ray generation groups.
    uint32_t raygen_count = 0;
    // Offset of the ray miss groups in elements
    uint32_t raymiss_offset = 0;
    // Amount of ray miss groups
    uint32_t raymiss_count = 0;
    // Offset of the ray hit groups in elements
    uint32_t rayhit_offset = 0;
    // Amount of ray hit groups
    uint32_t rayhit_count = 0;
    // Offset of the callable groups in elements
    uint32_t callable_offset = 0;
    // Amount of callable groups
    uint32_t callable_count = 0;

    // Group size in bytes. This is aligned to the correct alignment.
    uint32_t group_size_bytes = 0;

    VkDeviceAddress address{};
    std::array<VkStridedDeviceAddressRegionKHR, 4> regions;
};

#endif

class DescriptorBuilder {
public:
    static DescriptorBuilder create(Context& ctx, Pipeline const& pipeline);

    DescriptorBuilder& add_sampled_image(uint32_t binding, ImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DescriptorBuilder& add_sampled_image(ShaderMeta::Binding const& binding, ImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DescriptorBuilder& add_sampled_image(std::string_view binding, ImageView view, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    DescriptorBuilder& add_sampled_image_array(uint32_t binding, std::span<const ImageView> views, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DescriptorBuilder& add_sampled_image_array(ShaderMeta::Binding const& binding, std::span<const ImageView> views, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    DescriptorBuilder& add_sampled_image_array(std::string_view binding, std::span<const ImageView> views, VkSampler sampler, VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);


    DescriptorBuilder& add_storage_image(uint32_t binding, ImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
    DescriptorBuilder& add_storage_image(ShaderMeta::Binding const& binding, ImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
    DescriptorBuilder& add_storage_image(std::string_view binding, ImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);

    DescriptorBuilder& add_uniform_buffer(uint32_t binding, BufferSlice buffer);
    DescriptorBuilder& add_uniform_buffer(ShaderMeta::Binding const& binding, BufferSlice buffer);
    DescriptorBuilder& add_uniform_buffer(std::string_view binding, BufferSlice buffer);

    DescriptorBuilder& add_storage_buffer(uint32_t binding, BufferSlice buffer);
    DescriptorBuilder& add_storage_buffer(ShaderMeta::Binding const& binding, BufferSlice buffer);
    DescriptorBuilder& add_storage_buffer(std::string_view binding, BufferSlice buffer);

#if PHOBOS_ENABLE_RAY_TRACING
    DescriptorBuilder& add_acceleration_structure(uint32_t binding, AccelerationStructure const& as);
    DescriptorBuilder& add_acceleration_structure(ShaderMeta::Binding const& binding, AccelerationStructure const& as);
    DescriptorBuilder& add_acceleration_structure(std::string_view binding, AccelerationStructure const& as);
#endif

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
    PipelineBuilder& add_shader(std::string_view path, std::string_view entry, ShaderStage stage);
    PipelineBuilder& add_shader(ShaderHandle shader);
    PipelineBuilder& set_depth_test(bool test);
    PipelineBuilder& set_depth_write(bool write);
    PipelineBuilder& set_depth_op(VkCompareOp op);
    PipelineBuilder& set_depth_clamp(bool clamp);
    PipelineBuilder& add_dynamic_state(VkDynamicState state);
    PipelineBuilder& set_polygon_mode(VkPolygonMode mode);
    PipelineBuilder& set_cull_mode(VkCullModeFlags mode);
    PipelineBuilder& set_front_face(VkFrontFace face);
    PipelineBuilder& set_samples(VkSampleCountFlagBits samples);
    // enables sample shading and sets the min ratio
    PipelineBuilder& set_sample_shading(float value);
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

class ComputePipelineBuilder {
public:
    static ComputePipelineBuilder create(Context& ctx, std::string_view name);

    ComputePipelineBuilder& set_shader(ShaderHandle shader);
    ComputePipelineBuilder& set_shader(std::string_view path, std::string_view entry);
    ComputePipelineBuilder& reflect();

    ph::ComputePipelineCreateInfo get();
private:
    Context* ctx = nullptr;
    ph::ComputePipelineCreateInfo pci{};
};

#if PHOBOS_ENABLE_RAY_TRACING

class RayTracingPipelineBuilder {
public:
    static RayTracingPipelineBuilder create(Context& ctx, std::string_view name);

    RayTracingPipelineBuilder& add_shader(ShaderHandle shader);
    // Defines a shader group as a ray generation group with the supplied shader.
    // Shader must be a ray generation shader.
    RayTracingPipelineBuilder& add_ray_gen_group(ShaderHandle shader);
    // Defines a shader group as a ray miss group with the supplied shader.
    // Shader must be a ray miss shader
    RayTracingPipelineBuilder& add_ray_miss_group(ShaderHandle shader);
    // Defines a shader group as a ray hit group with the supplied shader.
    // Shader must be a closest hit shader.
    RayTracingPipelineBuilder& add_ray_hit_group(
        ShaderHandle closest_hit_shader,
        std::optional<ShaderHandle> anyhit_shader = std::nullopt);
    // Sets the maximum number level of recusion rays can have in this pipeline.
    RayTracingPipelineBuilder& set_recursion_depth(uint32_t depth);

    RayTracingPipelineBuilder& reflect();

    ph::RayTracingPipelineCreateInfo get();
private:
    Context* ctx = nullptr;
    ph::RayTracingPipelineCreateInfo pci{};
};

#endif

}