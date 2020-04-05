#ifndef PHOBOS_PRESENT_FRAME_INFO_HPP_
#define PHOBOS_PRESENT_FRAME_INFO_HPP_

#include <vulkan/vulkan.hpp>
#include <stl/vector.hpp>

#include <phobos/renderer/mapped_ubo.hpp>
#include <phobos/renderer/instancing_buffer.hpp>
#include <phobos/renderer/render_attachment.hpp>
#include <phobos/renderer/render_target.hpp>
#include <phobos/pipeline/pipeline.hpp>

namespace ph {

class PresentManager;
class RenderGraph;

// TODO: Clean up this struct + make stuff private

struct FrameInfo {
    RenderGraph* render_graph;
    PresentManager* present_manager;
    // Misc info. Do not modify
    size_t draw_calls;
    size_t frame_index;
    size_t image_index;
private:
    friend class RenderGraph;
    friend class PresentManager;
    friend class Renderer;

    // Synchronization
    vk::Fence fence;
    vk::Semaphore image_available;
    vk::Semaphore render_finished;

    // Other resources
    MappedUBO vp_ubo;
    InstancingBuffer transform_ssbo;
    MappedUBO lights;
    vk::Sampler default_sampler;

    vk::CommandBuffer command_buffer;

    // TODO: PerFrameCache class?
    Cache<vk::DescriptorSet, DescriptorSetBinding> descriptor_cache;
    vk::DescriptorPool descriptor_pool;
};

}

#endif