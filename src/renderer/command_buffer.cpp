#include <phobos/renderer/command_buffer.hpp>

#include <phobos/renderer/render_pass.hpp>
#include <phobos/present/frame_info.hpp>

namespace ph {

CommandBuffer::CommandBuffer(FrameInfo* frame, vk::CommandBuffer cbuf) : frame(frame), cmd_buf(cbuf) {

}

CommandBuffer& CommandBuffer::set_viewport(vk::Viewport const& vp) {
    STL_ASSERT(active_renderpass, "set_viewport called without an active renderpass");
    cmd_buf.setViewport(0, vp);
    return *this;
}

CommandBuffer& CommandBuffer::set_scissor(vk::Rect2D const& scissor) {
    STL_ASSERT(active_renderpass, "set_scissor called without an active renderpass");
    cmd_buf.setScissor(0, scissor);
    return *this;
}

CommandBuffer& CommandBuffer::bind_pipeline(Pipeline const& pipeline) {
    STL_ASSERT(active_renderpass, "bind_pipeline called without an active renderpass");
    active_renderpass->active_pipeline = pipeline;
    cmd_buf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.pipeline);
    return *this;
}

CommandBuffer& CommandBuffer::bind_descriptor_set(uint32_t first_binding, vk::DescriptorSet set, stl::span<uint32_t> dynamic_offsets) {
    STL_ASSERT(active_renderpass, "bind_descriptor_sets called without an active renderpass");
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, active_renderpass->active_pipeline.layout.layout, 
        first_binding, 1, &set, dynamic_offsets.size(), dynamic_offsets.begin());
    return *this;
}

CommandBuffer& CommandBuffer::bind_descriptor_sets(uint32_t first_binding, stl::span<vk::DescriptorSet> sets, stl::span<uint32_t> dynamic_offsets) {
    STL_ASSERT(active_renderpass, "bind_descriptor_sets called without an active renderpass");
    STL_ASSERT(sets.size() > 0, "bind_descriptor_sets called without descriptor sets");
    cmd_buf.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, active_renderpass->active_pipeline.layout.layout, 
        first_binding, sets.size(), &*sets.begin(), dynamic_offsets.size(), dynamic_offsets.begin());
    return *this;
}

CommandBuffer& CommandBuffer::bind_vertex_buffer(uint32_t first_binding, vk::Buffer buffer, vk::DeviceSize offset) {
    STL_ASSERT(active_renderpass, "bind_vertex_buffer called without an active renderpass");
    cmd_buf.bindVertexBuffers(first_binding, 1, &buffer, &offset);
    return *this;
}

CommandBuffer& CommandBuffer::bind_vertex_buffer(uint32_t first_binding, BufferSlice slice) {
    return bind_vertex_buffer(first_binding, slice.buffer, slice.offset);
}

CommandBuffer& CommandBuffer::bind_vertex_buffers(uint32_t first_binding, stl::span<vk::Buffer> buffers, stl::span<vk::DeviceSize> offsets) {
    STL_ASSERT(active_renderpass, "bind_vertex_buffers called without an active renderpass");
    STL_ASSERT(buffers.size() > 0, "bind_vertex_buffers called without buffers");
    STL_ASSERT(buffers.size() == offsets.size(), "bind_vertex_buffers called with an invalid number of offsets");

    cmd_buf.bindVertexBuffers(first_binding, buffers.size(), &*buffers.begin(), &*offsets.begin());
    return *this;
}

CommandBuffer& CommandBuffer::bind_index_buffer(vk::Buffer buffer, vk::DeviceSize offset, vk::IndexType type) {
    STL_ASSERT(active_renderpass, "bind_index_buffer called without an active renderpass");
    cmd_buf.bindIndexBuffer(buffer, offset, type);
    return *this;
}

CommandBuffer& CommandBuffer::bind_index_buffer(BufferSlice slice, vk::IndexType type) {
    return bind_index_buffer(slice.buffer, slice.offset, type);
}

CommandBuffer& CommandBuffer::push_constants(vk::ShaderStageFlags stage_flags, uint32_t offset, uint32_t size, void const* data) {
    STL_ASSERT(active_renderpass, "push_constants called without an active renderpass");
    cmd_buf.pushConstants(active_renderpass->active_pipeline.layout.layout, stage_flags, offset, size, data);
    return *this;
}

CommandBuffer& CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    STL_ASSERT(active_renderpass, "draw_ called without an active renderpass");
    cmd_buf.draw(vertex_count, instance_count, first_vertex, first_instance);
    return *this;
}

CommandBuffer& CommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index,
    uint32_t vertex_offset, uint32_t first_instance) {
    STL_ASSERT(active_renderpass, "draw_indexed called without an active renderpass");
    cmd_buf.drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
    return *this;
}

BufferSlice CommandBuffer::allocate_scratch_ubo(vk::DeviceSize size) {
    return frame->ubo_allocator.allocate(size);
}

BufferSlice CommandBuffer::allocate_scratch_ssbo(vk::DeviceSize size) {
    return frame->ssbo_allocator.allocate(size);
}

BufferSlice CommandBuffer::allocate_scratch_vbo(vk::DeviceSize size) {
    return frame->vbo_allocator.allocate(size);
}

BufferSlice CommandBuffer::allocate_scratch_ibo(vk::DeviceSize size) {
    return frame->ibo_allocator.allocate(size);
}

RenderPass* CommandBuffer::get_active_renderpass() {
    return active_renderpass;
}

CommandBuffer& CommandBuffer::begin(vk::CommandBufferUsageFlags usage_flags) {
    vk::CommandBufferBeginInfo info;
    info.flags = usage_flags;
    cmd_buf.begin(info);
    return *this;
}

CommandBuffer& CommandBuffer::end() {
    cmd_buf.end();
    return *this;
}

CommandBuffer& CommandBuffer::begin_renderpass(ph::RenderPass& pass) {
    active_renderpass = &pass;

    vk::RenderPassBeginInfo info;
    info.renderPass = pass.render_pass;
    info.framebuffer = pass.target.get_framebuf();
    info.renderArea.offset = vk::Offset2D { 0, 0 };
    info.renderArea.extent = vk::Extent2D { pass.target.get_width(), pass.target.get_height() };

    info.clearValueCount = pass.clear_values.size();
    info.pClearValues = pass.clear_values.data();

    cmd_buf.beginRenderPass(info, vk::SubpassContents::eInline);
    pass.active = true;

    return *this;
}

CommandBuffer& CommandBuffer::end_renderpass() {
    active_renderpass->active = false;
    cmd_buf.endRenderPass();
    return *this;
}

}