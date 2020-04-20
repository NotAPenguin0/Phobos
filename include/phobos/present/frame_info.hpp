#ifndef PHOBOS_PRESENT_FRAME_INFO_HPP_
#define PHOBOS_PRESENT_FRAME_INFO_HPP_

#include <vulkan/vulkan.hpp>
#include <stl/vector.hpp>

#include <phobos/renderer/dynamic_gpu_buffer.hpp>
#include <phobos/renderer/render_attachment.hpp>
#include <phobos/renderer/render_target.hpp>
#include <phobos/pipeline/pipeline.hpp>

#include <phobos/memory/buffer_allocator.hpp>

namespace ph {

class PresentManager;
class RenderGraph;

struct FrameInfo {
    RenderGraph* render_graph;
    PresentManager* present_manager;

    // Other resources
    RawBuffer vp_ubo;
    RawBuffer lights;
    RawBuffer skybox_ubo;
    DynamicGpuBuffer transform_ssbo;

    // Non-owning
    vk::Sampler default_sampler;

    // Misc info. Do not modify
    size_t draw_calls;
    size_t frame_index;
    size_t image_index;
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

    // TODO: PerFrameCache class?
    Cache<vk::DescriptorSet, DescriptorSetBinding> descriptor_cache;
    // Non-owning
    vk::DescriptorPool descriptor_pool;

    BufferAllocator ubo_allocator;
};

}

#endif