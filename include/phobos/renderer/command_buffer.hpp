#ifndef PHOBOS_COMMAND_BUFFER_HPP_
#define PHOBOS_COMMAND_BUFFER_HPP_

#include <phobos/core/vulkan_context.hpp>

#include <phobos/pipeline/pipeline.hpp>

namespace ph {

struct RenderPass;
struct FrameInfo;

class CommandBuffer {
public:
    CommandBuffer(VulkanContext* ctx, FrameInfo* frame, PerThreadContext* ptc, vk::CommandBuffer cbuf);

    // The descriptor set layout will be set by this function. The pNext parameter will be passed into the vk::DescriptorSetAllocateInfo::pNext
    // field.
    vk::DescriptorSet get_descriptor(DescriptorSetBinding set_binding, void* pNext = nullptr);
    Pipeline get_pipeline(std::string_view name);
    Pipeline get_compute_pipeline(std::string_view name);

    CommandBuffer& set_viewport(vk::Viewport const& vp);
    CommandBuffer& set_scissor(vk::Rect2D const& scissor);

    CommandBuffer& bind_pipeline(Pipeline const& pipeline);
    CommandBuffer& bind_descriptor_set(uint32_t first_binding, vk::DescriptorSet set, stl::span<uint32_t> dynamic_offsets = {});
    CommandBuffer& bind_descriptor_sets(uint32_t first_binding, stl::span<vk::DescriptorSet> sets, stl::span<uint32_t> dynamic_offsets = {});
    CommandBuffer& bind_vertex_buffer(uint32_t first_binding, vk::Buffer buffer, vk::DeviceSize offset = 0);
    CommandBuffer& bind_vertex_buffer(uint32_t first_binding, BufferSlice slice);
    CommandBuffer& bind_vertex_buffers(uint32_t first_binding, stl::span<vk::Buffer> buffers, stl::span<vk::DeviceSize> offsets);
    CommandBuffer& bind_index_buffer(vk::Buffer buffer, vk::DeviceSize offset = 0, vk::IndexType type = vk::IndexType::eUint32);
    CommandBuffer& bind_index_buffer(BufferSlice slice, vk::IndexType type = vk::IndexType::eUint32);
    CommandBuffer& push_constants(vk::ShaderStageFlags stage_flags, uint32_t offset, uint32_t size, void const* data);
    CommandBuffer& draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex = 0, uint32_t first_instance = 0);
    CommandBuffer& draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index = 0, 
        uint32_t vertex_offset = 0, uint32_t first_instance = 0);
    CommandBuffer& dispatch_compute(uint32_t workgroup_x, uint32_t workgroup_y, uint32_t workgroup_z);

    CommandBuffer& barrier(vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage, vk::ImageMemoryBarrier barrier);
    CommandBuffer& barrier(vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage, vk::BufferMemoryBarrier barrier);
    CommandBuffer& blit_image(RawImage& src_image, vk::ImageLayout src_layout, RawImage& dst_image, vk::ImageLayout dst_layout,
        vk::ImageBlit blit_region, vk::Filter filter = vk::Filter::eLinear);
    CommandBuffer& blit_image(RawImage& src_image, vk::ImageLayout src_layout, RawImage& dst_image, vk::ImageLayout dst_layout,
        stl::span<vk::ImageBlit> blit_regions, vk::Filter filter = vk::Filter::eLinear);

    BufferSlice allocate_scratch_ubo(vk::DeviceSize size);
    BufferSlice allocate_scratch_ssbo(vk::DeviceSize size);
    BufferSlice allocate_scratch_vbo(vk::DeviceSize size);
    BufferSlice allocate_scratch_ibo(vk::DeviceSize size);

    RenderPass* get_active_renderpass();

    CommandBuffer& begin(vk::CommandBufferUsageFlags usage_flags);
    CommandBuffer& end();

    CommandBuffer& begin_renderpass(RenderPass& pass);
    CommandBuffer& end_renderpass();
private:
    friend class Renderer;

    VulkanContext* ctx = nullptr;
    FrameInfo* frame = nullptr;
    PerThreadContext* ptc = nullptr;
    vk::CommandBuffer cmd_buf;
    RenderPass* active_renderpass = nullptr;
    Pipeline active_pipeline;

    // Called internally by get_descriptor to actually handle the descriptor creation/updates
    vk::DescriptorSet get_descriptor(DescriptorSetLayoutCreateInfo const& set_layout_info, DescriptorSetBinding set_binding, void* pNext = nullptr);
};

}

#endif