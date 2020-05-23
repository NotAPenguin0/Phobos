#ifndef PHOBOS_PRESENT_FRAME_INFO_HPP_
#define PHOBOS_PRESENT_FRAME_INFO_HPP_

#include <vulkan/vulkan.hpp>
#include <stl/vector.hpp>

#include <phobos/renderer/render_attachment.hpp>
#include <phobos/renderer/render_target.hpp>
#include <phobos/pipeline/pipeline.hpp>

#include <phobos/memory/buffer_allocator.hpp>

namespace ph {

class PresentManager;
class RenderGraph;

struct FrameInfo {
    FrameInfo() = default;
    FrameInfo(FrameInfo const&) = delete;
    FrameInfo(FrameInfo&&) = default;
    FrameInfo& operator=(FrameInfo&&) = default;
    FrameInfo& operator=(FrameInfo const&) = delete;

    RenderGraph* render_graph;
    PresentManager* present_manager;

    // Non-owning
    vk::Sampler default_sampler;

    // Misc info. Do not modify
    size_t draw_calls = 0;
    size_t frame_index = 0;
    size_t image_index = 0;

    BufferAllocator ubo_allocator;
    BufferAllocator ssbo_allocator;
    BufferAllocator vbo_allocator;
    BufferAllocator ibo_allocator;
private:
    friend class RenderGraph;
    friend class PresentManager;
    friend class Renderer;
    friend class CommandBuffer;

    // Synchronization
    vk::Fence fence;
    vk::Semaphore image_available;
    vk::Semaphore render_finished;

    vk::CommandBuffer command_buffer;

    Cache<vk::DescriptorSet, DescriptorSetBinding> descriptor_cache;
};

}

#endif