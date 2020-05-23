#ifndef PHOBOS_VULKAN_CONTEXT_HPP_
#define PHOBOS_VULKAN_CONTEXT_HPP_

#include <vulkan/vulkan.hpp>

#include <phobos/version.hpp>
#include <phobos/core/window_context.hpp>
#include <phobos/core/physical_device.hpp>
#include <phobos/core/swapchain.hpp>

#include <phobos/pipeline/pipeline.hpp>

#include <phobos/util/log_interface.hpp>

#include <phobos/events/event_dispatcher.hpp>

#include <phobos/util/cache.hpp>
#include <phobos/util/shader_util.hpp>

#include <vk_mem_alloc.h>

#include <phobos/core/queue.hpp>

namespace ph {

struct PerThreadContext {
    vk::DescriptorPool descriptor_pool;
    Cache<vk::RenderPass, vk::RenderPassCreateInfo> renderpass_cache;
    Cache<vk::Framebuffer, vk::FramebufferCreateInfo> framebuffer_cache;
    Cache<vk::Pipeline, ph::PipelineCreateInfo> pipeline_cache;
    Cache<vk::Pipeline, ph::ComputePipelineCreateInfo> compute_pipeline_cache;
    Cache<ph::PipelineLayout, ph::PipelineLayoutCreateInfo> pipeline_layout_cache;
    Cache<vk::DescriptorSetLayout, ph::DescriptorSetLayoutCreateInfo> set_layout_cache;
};

struct VulkanContext {
    vk::Instance instance;
    vk::DispatchLoaderDynamic dynamic_dispatcher;
    // Only available if enable_validation_layers is set to true
    vk::DebugUtilsMessengerEXT debug_messenger;

    bool has_validation = false;

    PhysicalDeviceDetails physical_device;
    vk::Device device;

    WindowContext* window_ctx;

    uint32_t num_threads;
    std::vector<PerThreadContext> thread_contexts;

    std::unique_ptr<Queue> graphics;
    std::unique_ptr<Queue> compute;
    std::unique_ptr<Queue> transfer;
    // Queue compute

    SwapchainDetails swapchain;
    PipelineManager pipelines;
    
    log::LogInterface* logger = nullptr;
    EventDispatcher event_dispatcher;

    Cache<ph::ShaderModuleCreateInfo, ShaderHandle> shader_module_info_cache;

    VmaAllocator allocator;

    void destroy();
};

struct AppSettings {
    bool enable_validation_layers = false;
    Version version = Version {1, 0, 0};
    // Using more threads than num_threads is undefined and may lead to race conditions or crashes
    uint32_t num_threads = 1;
};

VulkanContext* create_vulkan_context(WindowContext& window_ctx, log::LogInterface* logger, AppSettings settings = {});
void destroy_vulkan_context(VulkanContext* ctx);

}

#endif