#include <phobos/renderer/command_buffer.hpp>

#include <phobos/renderer/render_pass.hpp>
#include <phobos/present/frame_info.hpp>

namespace ph {

static vk::PipelineBindPoint pipeline_bind_point(Pipeline const& pipeline) {
    switch (pipeline.type) {
    case PipelineType::Graphics:
        return vk::PipelineBindPoint::eGraphics;
    case PipelineType::Compute:
        return vk::PipelineBindPoint::eCompute;
    }
}

CommandBuffer::CommandBuffer(VulkanContext* ctx, FrameInfo* frame, PerThreadContext* ptc, vk::CommandBuffer cbuf) 
    : ctx(ctx), frame(frame), ptc(ptc), cmd_buf(cbuf) {

}

vk::DescriptorSet CommandBuffer::get_descriptor(DescriptorSetBinding set_binding, void* pNext) {
    if (active_pipeline.type == PipelineType::Graphics) {
        auto const& set_layout_info = ctx->pipelines.get_named_pipeline(active_pipeline.name)->layout.set_layout;
        return get_descriptor(set_layout_info, stl::move(set_binding), pNext);
    } else if (active_pipeline.type == PipelineType::Compute) {
        auto const& set_layout_info = ctx->pipelines.get_named_compute_pipeline(active_pipeline.name)->layout.set_layout;
        return get_descriptor(set_layout_info, stl::move(set_binding), pNext);
    }
    return nullptr;
}

Pipeline CommandBuffer::get_pipeline(std::string_view name) {
    ph::PipelineCreateInfo const* pci = ctx->pipelines.get_named_pipeline(std::string(name));
    STL_ASSERT(pci, "Pipeline not found");
    STL_ASSERT(active_renderpass, "get_pipeline without an active renderpass");
    Pipeline pipeline = create_or_get_pipeline(ctx, ptc, active_renderpass, *pci);
    pipeline.name = name;
    return pipeline;
}

Pipeline CommandBuffer::get_compute_pipeline(std::string_view name) {
    ph::ComputePipelineCreateInfo const* pci = ctx->pipelines.get_named_compute_pipeline(std::string(name));
    STL_ASSERT(pci, "Pipeline not found");
    Pipeline pipeline = create_or_get_compute_pipeline(ctx, ptc, *pci);
    pipeline.name = name;
    return pipeline;
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
    active_pipeline = pipeline;
    cmd_buf.bindPipeline(pipeline_bind_point(pipeline), pipeline.pipeline);
    return *this;
}

CommandBuffer& CommandBuffer::bind_descriptor_set(uint32_t first_binding, vk::DescriptorSet set, stl::span<uint32_t> dynamic_offsets) {
    cmd_buf.bindDescriptorSets(pipeline_bind_point(active_pipeline), active_pipeline.layout.layout,
        first_binding, 1, &set, dynamic_offsets.size(), dynamic_offsets.begin());
    return *this;
}

CommandBuffer& CommandBuffer::bind_descriptor_sets(uint32_t first_binding, stl::span<vk::DescriptorSet> sets, stl::span<uint32_t> dynamic_offsets) {
    STL_ASSERT(sets.size() > 0, "bind_descriptor_sets called without descriptor sets");
    cmd_buf.bindDescriptorSets(pipeline_bind_point(active_pipeline), active_pipeline.layout.layout,
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
    cmd_buf.pushConstants(active_pipeline.layout.layout, stage_flags, offset, size, data);
    return *this;
}

CommandBuffer& CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
    STL_ASSERT(active_renderpass, "draw called without an active renderpass");
    cmd_buf.draw(vertex_count, instance_count, first_vertex, first_instance);
    frame->draw_calls++;
    return *this;
}

CommandBuffer& CommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index,
    uint32_t vertex_offset, uint32_t first_instance) {
    STL_ASSERT(active_renderpass, "draw_indexed called without an active renderpass");
    cmd_buf.drawIndexed(index_count, instance_count, first_index, vertex_offset, first_instance);
    frame->draw_calls++;
    return *this;
}

CommandBuffer& CommandBuffer::dispatch_compute(uint32_t workgroup_x, uint32_t workgroup_y, uint32_t workgroup_z) {
    cmd_buf.dispatch(workgroup_x, workgroup_y, workgroup_z);
    return *this;
}

CommandBuffer& CommandBuffer::barrier(vk::PipelineStageFlags src_stage, vk::PipelineStageFlags dst_stage, vk::ImageMemoryBarrier barrier) {
    cmd_buf.pipelineBarrier(src_stage, dst_stage, vk::DependencyFlagBits::eByRegion, nullptr, nullptr, barrier);
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

vk::DescriptorSet CommandBuffer::get_descriptor(DescriptorSetLayoutCreateInfo const& set_layout_info, DescriptorSetBinding set_binding, void* pNext) {
    set_binding.set_layout = set_layout_info;
    auto set_opt = frame->descriptor_cache.get(set_binding);
    if (!set_opt) {
        // Create descriptor set layout and issue writes
        vk::DescriptorSetAllocateInfo alloc_info;
        // Get set layout. We can assume that it has already been created. If not, something went very wrong
        vk::DescriptorSetLayout* set_layout = ptc->set_layout_cache.get(set_binding.set_layout);
        STL_ASSERT(set_layout, "Descriptor requested without creating desctiptor set layout first. This can happen if there is"
            " no active pipeline bound, or get_descriptor was called outside a valid ph::RenderPass callback.");
        alloc_info.pSetLayouts = set_layout;
        // add default descriptor pool if no custom one was specified
        if (!set_binding.pool) { set_binding.pool = ptc->descriptor_pool; };
        alloc_info.descriptorPool = set_binding.pool;
        alloc_info.descriptorSetCount = 1;
        alloc_info.pNext = pNext;

        // allocateDescriptorSets returns an array of descriptor sets, but we only allocate one, so we need to index it with 0
        vk::DescriptorSet set = ctx->device.allocateDescriptorSets(alloc_info)[0];

        // Now we have the set we need to write the requested data to it
        // TODO: Look into vkUpdateDescriptorSetsWithTemplate?
        stl::vector<vk::WriteDescriptorSet> writes;
        struct DescriptorWriteInfo {
            stl::vector<vk::DescriptorBufferInfo> buffer_infos;
            stl::vector<vk::DescriptorImageInfo> image_infos;
        };
        stl::vector<DescriptorWriteInfo> write_infos;
        writes.reserve(set_binding.bindings.size());
        for (auto const& binding : set_binding.bindings) {
            if (binding.descriptors.empty()) continue;
            DescriptorWriteInfo write_info;
            vk::WriteDescriptorSet write;
            write.dstSet = set;
            write.dstBinding = binding.binding;
            write.descriptorType = binding.type;
            write.descriptorCount = binding.descriptors.size();
            switch (binding.type) {
            case vk::DescriptorType::eStorageImage:
            case vk::DescriptorType::eCombinedImageSampler:
            case vk::DescriptorType::eSampledImage: {
                write_info.image_infos.reserve(binding.descriptors.size());
                for (auto const& descriptor : binding.descriptors) {
                    vk::DescriptorImageInfo img;
                    img.imageLayout = descriptor.image.layout;
                    img.imageView = descriptor.image.view.view;
                    img.sampler = descriptor.image.sampler;
                    write_info.image_infos.push_back(img);
                    // Push dummy buffer info to make sure indices match
                    write_info.buffer_infos.emplace_back();
                }
            } break;
            default: {
                vk::DescriptorBufferInfo buf;
                auto& info = binding.descriptors.front().buffer;
                buf.buffer = info.buffer;
                buf.offset = info.offset;
                buf.range = info.range;

                write_info.buffer_infos.push_back(buf);
                // Push dummy image info to make sure indices match
                write_info.image_infos.emplace_back();
            } break;
            }
            write_infos.push_back(write_info);
            writes.push_back(stl::move(write));
        }

        for (size_t i = 0; i < write_infos.size(); ++i) {
            switch (writes[i].descriptorType) {
            case vk::DescriptorType::eStorageImage:
            case vk::DescriptorType::eSampledImage:
            case vk::DescriptorType::eCombinedImageSampler: {
                writes[i].pImageInfo = write_infos[i].image_infos.data();
            } break;
            default: {
                writes[i].pBufferInfo = write_infos[i].buffer_infos.data();
            } break;
            }
        }

        ctx->device.updateDescriptorSets(writes.size(), writes.data(), 0, nullptr);

        // Insert into cache and return the set to the user
        vk::DescriptorSet backup = set;
        frame->descriptor_cache.insert(set_binding, stl::move(backup));
        return set;
    }
    else {
        return *set_opt;
    }
}

}